"""Print or save a diagnostic TXT from an ESP-IDF core dump without runtime strings.

Run with PlatformIO's Python, which supplies esp_coredump and pygdbmi.
"""

import argparse
import contextlib
import io
import json
import re
import struct
import sys
import time
import traceback
from pathlib import Path

from esp_coredump import CoreDump
from esp_coredump.corefile import RISCV_TARGETS, SUPPORTED_TARGETS, XTENSA_TARGETS
from esp_coredump.corefile.elf import ESPCoreDumpElfFile
from esp_coredump.corefile.loader import EspCoreDumpLoader, EspCoreDumpV1Header, EspCoreDumpVersion
from esp_coredump.corefile.xtensa import print_exc_regs_info
from pygdbmi.gdbcontroller import GdbController


HEAP_INFO_FIELDS = (
  ('total_free_bytes', 'Total free', ' bytes'),
  ('total_allocated_bytes', 'Total allocated', ' bytes'),
  ('largest_free_block', 'Largest free block', ' bytes'),
  ('minimum_free_bytes', 'Minimum free (SDK counter)', ' bytes'),
  ('allocated_blocks', 'Allocated blocks', ''),
  ('free_blocks', 'Free blocks', ''),
  ('total_blocks', 'Total blocks', ''),
)


class AnalysisError(Exception):
  """A diagnostic that contains only known tool/target identifiers."""


class Mi:
  def __init__(self, command):
    self.controller = GdbController(command=command)
    self.token = 0

  def command(self, command):
    self.token += 1
    token = self.token
    self.controller.write(f'{token}{command}', read_response=False)
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
      responses = self.controller.get_gdb_response(timeout_sec=0.2, raise_error_on_timeout=False)
      for response in responses:
        if response.get('type') == 'result' and response.get('token') == token:
          if response.get('message') == 'error':
            return None
          return response.get('payload') or {}
    raise TimeoutError('GDB/MI command timed out')

  def number(self, expression):
    result = self.command('-data-evaluate-expression ' + json.dumps(expression))
    if result is None:
      return None
    value = result.get('value', '')
    if re.fullmatch(r'(?:0x[0-9a-fA-F]+|[0-9]+)', value):
      return int(value, 0)
    return None

  def close(self):
    # Exit through MI so Windows launchers also wait for the real GDB process.
    self.command('-gdb-exit')
    self.controller.gdb_process.wait(timeout=5)
    self.controller.exit()


def saved_bytes(core, address, size):
  for segment in core.load_segments:
    offset = address - segment.addr
    if offset >= 0 and offset + size <= len(segment.data):
      return segment.data[offset:offset + size]
  return None


def saved_number(mi, core, expression):
  address = mi.number(f'(unsigned long)&{expression}')
  size = mi.number(f'sizeof({expression})')
  raw = saved_bytes(core, address, size) if address is not None and size else None
  return int.from_bytes(raw, 'little') if raw is not None else None


def crash_metrics(mi, core):
  crash_address = mi.number('(unsigned long)&crashMetrics')
  memory_address = mi.number('(unsigned long)&memoryMetrics')
  if crash_address is not None or memory_address is not None:
    captured = saved_number(mi, core, 'crashMetrics.captured') if crash_address is not None else None
    memory_captured = saved_number(mi, core, 'memoryMetrics.captured') if memory_address is not None else None
    values = {'source': 'crashMetrics + memoryMetrics', 'captured': captured, 'heapCaptured': memory_captured,
              'core': saved_number(mi, core, 'crashMetrics.core') if captured == 1 else None,
              'uptime': saved_number(mi, core, 'crashMetrics.uptime') if captured == 1 else None,
              'heapUptime': saved_number(mi, core, 'memoryMetrics.uptime') if memory_captured == 1 else None}
    for heap in ('heap', 'psram'):
      values[heap] = {name: saved_number(mi, core, f'memoryMetrics.{heap}.{name}') if memory_captured == 1 else None
                      for name, _, _ in HEAP_INFO_FIELDS}
    return values
  if mi.number('(unsigned long)&coreDumpMetrics') is not None:
    if mi.number('(unsigned long)&coreDumpMetrics.heap.total_free_bytes') is not None:
      names = ('captured', 'core', 'uptime', 'heapUptime')
      if mi.number('(unsigned long)&coreDumpMetrics.heapCaptured') is not None:
        names += ('heapCaptured',)
      values = {name: saved_number(mi, core, f'coreDumpMetrics.{name}') for name in names}
      for heap in ('heap', 'psram'):
        values[heap] = {name: saved_number(mi, core, f'coreDumpMetrics.{heap}.{name}')
                        for name, _, _ in HEAP_INFO_FIELDS}
      if values['captured'] != 1:
        for name in names:
          values[name] = None
      if values['captured'] != 1 or values.get('heapCaptured', 1) != 1:
        values['heapUptime'] = None
        for heap in ('heap', 'psram'):
          values[heap] = {name: None for name, _, _ in HEAP_INFO_FIELDS}
      return {'source': 'coreDumpMetrics', **values}
    captured = mi.number('(unsigned long)&coreDumpMetrics.captured') is not None
    names = (('captured', 'heap', 'minimumHeap', 'core', 'uptime') if captured
             else ('version', 'heap', 'minimumHeap', 'core', 'uptimeUs'))
    values = {name: saved_number(mi, core, f'coreDumpMetrics.{name}') for name in names}
    # The record is valid only after the current panic hook completes it.
    if values['captured' if captured else 'version'] != 1:
      values = {name: None for name in names}
    return {'source': 'coreDumpMetrics', **values}
  return {'source': 'CrashRecorder::ext', **{
    name: saved_number(mi, core, f'CrashRecorder::ext.{name}') for name in ('heap', 'core', 'uptime')}}


def build_info(mi, core):
  patterns = {'version': r'[A-Za-z0-9][A-Za-z0-9_.+ /:-]{0,95}',
              'date': r'[A-Z][a-z]{2} {1,2}[0-9]{1,2} [0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2}',
              'commit': r'(?:[0-9a-fA-F]{7,64}|undefined|unknown)(?:-dirty)?',
              'env': r'[A-Za-z0-9][A-Za-z0-9_.+-]{0,95}',
              'core': r'[A-Za-z0-9][A-Za-z0-9_.+ /:-]{0,95}',
              'sdk': r'[A-Za-z0-9][A-Za-z0-9_.+ /:-]{0,95}',
              'chipModel': r'ESP32(?:-[A-Za-z0-9]+)*'}
  values = {}
  for name, pattern in patterns.items():
    expression = f'coreDumpBuildInfo.{name}'
    address = mi.number(f'(unsigned long)&{expression}')
    size = mi.number(f'sizeof({expression})')
    raw = saved_bytes(core, address, size) if address is not None and size and size <= 128 else None
    # Compile-time and SDK identifiers are bounded; SDK buffers can be zero-padded.
    value = raw.split(b'\0', 1)[0].decode('ascii', errors='replace') if raw and b'\0' in raw else None
    values[name] = value if value is not None and re.fullmatch(pattern, value) else None
  for name in ('chipRevision', 'chipCores', 'cpuFreqMHz', 'flashChipSize'):
    values[name] = saved_number(mi, core, f'coreDumpBuildInfo.{name}')
  return values


def render_build_info(build):
  lines = []
  for name, label, unit in (('version', 'Firmware version', ''), ('date', 'Build date/time', ''),
                            ('commit', 'Git commit', ''), ('env', 'PlatformIO environment', ''),
                            ('core', 'Arduino core version', ''), ('sdk', 'ESP-IDF SDK version', ''),
                            ('chipModel', 'Chip model', ''), ('chipRevision', 'Chip revision', ''),
                            ('chipCores', 'Chip CPU cores', ''), ('cpuFreqMHz', 'CPU frequency at boot', ' MHz'),
                            ('flashChipSize', 'Flash chip size', ' bytes')):
    value = build[name]
    lines.append(f'{label}: ' + (f'{value}{unit}' if value is not None else 'not captured or invalid'))
  lines.append('Source: coreDumpBuildInfo; build and boot hardware identifiers are read only from saved PT_LOAD bytes.')
  return lines


def render_metrics(metrics):
  lines = []
  sampled = isinstance(metrics['heap'], dict)
  labels = {'core': 'CPU core at panic'} if sampled else {'heap': 'Free internal heap at panic', 'core': 'CPU core at panic'}
  if metrics['source'] != 'CrashRecorder::ext' and not sampled:
    labels['minimumHeap'] = 'Minimum free internal heap since boot (SDK counter)'
  for name, label in labels.items():
    value = metrics[name]
    unit = ' bytes' if name in ('heap', 'minimumHeap') else ''
    lines.append(f'{label}: ' + (f'{value}{unit}' if value is not None else 'not captured'))
  if metrics['source'] != 'CrashRecorder::ext':
    if 'uptime' in metrics:
      uptime = metrics['uptime']
      rendered = f'{uptime} milliseconds' if uptime is not None else 'not captured'
    else:
      uptime = metrics['uptimeUs']
      rendered = f'{uptime // 1000}.{uptime % 1000:03} milliseconds (legacy microsecond record)' if uptime is not None else 'not captured'
    lines.append('Uptime at panic: ' + rendered)
    if sampled:
      if metrics.get('heapCaptured', 1) != 1:
        lines.append('Heap statistics not captured: update incomplete or unavailable.')
      sampled_at = metrics['heapUptime']
      lines.append('Heap snapshot uptime: ' + (f'{sampled_at} milliseconds' if sampled_at is not None else 'not captured'))
      age = uptime - sampled_at if uptime is not None and sampled_at is not None else None
      rendered_age = f'{age} milliseconds' if age is not None and age >= 0 else 'not available'
      if age is not None and age < 0:
        rendered_age = 'not available (snapshot timestamp is after panic)'
      lines += ['Heap snapshot age at panic: ' + rendered_age,
                'Heap statistics are the last completed snapshot, not measurements taken in the panic handler.']
      for name, label in (('heap', 'INTERNAL HEAP'), ('psram', 'PSRAM HEAP')):
        info = metrics[name]
        lines += ['', label + ' (last completed snapshot)']
        if name == 'psram' and info['total_free_bytes'] == 0 and info['total_allocated_bytes'] == 0:
          lines.append('Not available (no registered PSRAM heap in the snapshot).')
          continue
        for field, description, unit in HEAP_INFO_FIELDS:
          value = info[field]
          lines.append(f'{description}: ' + (f'{value}{unit}' if value is not None else 'not captured'))
  else:
    uptime = metrics['uptime']
    lines.append('Uptime at panic: ' + (f'{uptime} seconds' if uptime is not None else 'not captured'))
    lines.append('Legacy metrics: uptime uses the original 32-bit millis()/1000 semantics; minimum heap was not recorded.')
  lines.append(f"Source: {metrics['source']}; values are read only from saved PT_LOAD bytes, never substituted from the firmware ELF or a running device.")
  return lines


def task_name(mi, tcb, firmware):
  expression = f'(char*)((TCB_t*){tcb})->pcTaskName'
  result = mi.command('-data-evaluate-expression ' + json.dumps(expression))
  match = re.search(r'"([A-Za-z0-9_ .:/-]{1,16})"$', result.get('value', '')) if result else None
  if match:
    name = match.group(1)
    # Kernel idle names can be formatted at runtime; other names must occur in the ELF.
    if name in {'IDLE0', 'IDLE1'} or name.encode() + b'\0' in firmware:
      return name
  return 'not captured or not a known static task name'


def stack_frames(mi, thread):
  result = mi.command(f'-stack-list-frames --thread {thread}')
  if result is None:
    return []
  return [item.get('frame', item) for item in result.get('stack', [])]


def render_frame(frame):
  location = frame.get('fullname') or frame.get('file')
  if location and frame.get('line'):
    location += ':' + str(frame['line'])
  return (f"#{frame['level']}  {frame.get('addr', '<address unavailable>')}  "
          f"{frame.get('func', '<symbol unavailable>')}"
          + (' at ' + location if location else ' [no DWARF source location]'))


def code_context(frames, directory):
  if directory is None:
    return []
  for frame in frames:
    source = frame.get('fullname') or frame.get('file', '')
    basename = source.replace('\\', '/').rsplit('/', 1)[-1]
    if basename != 'system.cpp' or not frame.get('line'):
      continue
    path = directory / basename
    if not path.is_file():
      continue
    lines = path.read_text(encoding='utf-8').splitlines()
    number = int(frame['line'])
    start = max(0, number - 9)
    end = min(len(lines), number + 3)
    selected = lines[start:end]
    # Only emit the deliberately authored test block, never unrelated source/configuration.
    if not any('OTGateway native dump test ' in line for line in selected):
      continue
    if not any('esp_system_abort(' in line for line in selected):
      continue
    result = [f'DWARF location: {source}:{number}', f'Source snapshot: {path.resolve()}']
    result += [f'{index + 1:5}: {lines[index]}' for index in range(start, end)]
    return result
  return ['Test source context not available in the supplied source-map directory.']


def target_machine(target):
  if target in XTENSA_TARGETS:
    return ESPCoreDumpElfFile.EM_XTENSA
  if target in RISCV_TARGETS:
    return ESPCoreDumpElfFile.EM_RISCV
  raise AnalysisError('The installed esp-coredump does not support this dump target.')


def native_target(path):
  with path.open('rb') as source:
    header = source.read(EspCoreDumpV1Header.sizeof())
  if len(header) != EspCoreDumpV1Header.sizeof():
    raise AnalysisError('The native core dump header is incomplete.')
  version = EspCoreDumpVersion(EspCoreDumpV1Header.parse(header).ver)
  targets = {getattr(EspCoreDumpVersion, target.upper()): target for target in SUPPORTED_TARGETS}
  if version.chip_ver not in targets:
    raise AnalysisError('The native dump chip is not supported by the installed esp-coredump. Update the decoder.')
  if version.dump_ver not in EspCoreDumpLoader.CORE_VERSIONS:
    raise AnalysisError('The native dump format is not supported by the installed esp-coredump.')
  return targets[version.chip_ver]


def validate_target(target, requested, elf_machine, core_machine=None):
  expected = target_machine(target)
  if requested != 'auto' and requested != target:
    raise AnalysisError(f'--chip {requested} does not match dump target {target}.')
  if elf_machine != expected or (core_machine is not None and core_machine != expected):
    raise AnalysisError(f'Firmware/core ELF architecture does not match dump target {target}. Use the exact matching firmware ELF.')
  return target


def default_gdb(platformio_home, target):
  suffix = '.exe' if sys.platform == 'win32' else ''
  if target in XTENSA_TARGETS:
    directory = platformio_home / 'packages/tool-xtensa-esp-elf-gdb/bin'
    names = (f'xtensa-{target}-elf-gdb', 'xtensa-esp-elf-gdb', 'xtensa-esp32-elf-gdb')
  else:
    directory = platformio_home / 'packages/tool-riscv32-esp-elf-gdb/bin'
    names = ('riscv32-esp-elf-gdb',)
  candidates = [directory / (name + suffix) for name in names]
  return next((path for path in candidates if path.is_file()), candidates[0])


def default_rom(platformio_home, target, chip_rev):
  if chip_rev is None:
    return None
  directory = platformio_home / 'packages/tool-esp-rom-elfs'
  candidates = []
  for path in directory.glob(f'{target}_rev*_rom.elf'):
    match = re.fullmatch(rf'{re.escape(target)}_rev([0-9]+)_rom\.elf', path.name)
    if match and int(match.group(1)) <= chip_rev:
      candidates.append((int(match.group(1)), path))
  return max(candidates)[1] if candidates else None


def gdb_arguments(dump, header, rom):
  command = dump.get_gdb_args(is_dbg_mode=False, **header)
  if rom is not None:
    # SDK's add-symbol-file command omits filename quotes; support paths with spaces.
    for index, argument in enumerate(command):
      if argument.startswith('add-symbol-file '):
        address = argument.rsplit(' ', 1)[-1]
        command[index] = f'add-symbol-file {json.dumps(rom.as_posix())} {address}'
  return command


def report(args):
  program = ESPCoreDumpElfFile(str(args.elf))
  if program.e_machine not in (ESPCoreDumpElfFile.EM_XTENSA, ESPCoreDumpElfFile.EM_RISCV):
    raise AnalysisError('Firmware ELF must target an ESP32 Xtensa or RISC-V CPU.')
  validate_target(native_target(args.dump), args.chip, program.e_machine)
  dump = CoreDump(chip=args.chip, core=str(args.dump), core_format='raw', prog=str(args.elf))
  temporary = []
  mi = None
  try:
    header = dump.get_core_header_info_dict(e_machine=program.e_machine)
    temporary = header.pop('temp_files') or []
    dump.core_elf = ESPCoreDumpElfFile(header['core_elf_path'])
    target = validate_target(header['target'], args.chip, program.e_machine, dump.core_elf.e_machine)
    gdb = args.gdb or default_gdb(args.platformio_home, target)
    if not gdb.is_file():
      raise AnalysisError(f'GDB for {target} was not found; use --gdb or --platformio-home to select installed tools.')
    dump.gdb = str(gdb)
    rom = args.rom or default_rom(args.platformio_home, target, header['chip_rev'])
    dump.rom_elf = str(rom) if rom is not None else None
    if rom is None:
      resolved = dump.get_rom_elf_path(target=target, chip_rev=header['chip_rev'])
      rom = Path(resolved) if resolved and Path(resolved).is_file() else None
      dump.rom_elf = str(rom) if rom is not None else None
    if rom is not None and ESPCoreDumpElfFile(str(rom)).e_machine != program.e_machine:
      raise AnalysisError(f'ROM ELF architecture does not match dump target {target}. Select a target-compatible --rom.')
    xtensa = target_machine(target) == ESPCoreDumpElfFile.EM_XTENSA
    task_info, extra = dump.get_task_info_extra_note_tuple()
    panic = dump.get_panic_details()
    reason = panic.desc.decode('utf-8', errors='replace') if panic else ''
    known_reason = re.fullmatch(
      r'OTGateway native dump test OTG-S3-[0-9]{8}-r[0-9]+; uptime_us=([0-9]+); marker=43525348', reason)
    low_heap = reason == 'OTGateway: low internal heap'
    startup_failure = re.fullmatch(r'Failed to start (?:OpenTherm|Core|System) task manager', reason)
    words = struct.unpack('<' + 'I' * (len(extra.desc) // 4), extra.desc) if extra else ()
    crashed_tcb = words[0] if words else None
    mi = Mi(gdb_arguments(dump, header, rom))
    architecture = 'xtensa' if xtensa else 'riscv:rv32'
    if mi.command('-gdb-set architecture ' + architecture) is None:
      raise AnalysisError(f'The selected GDB architecture does not match dump target {target}. Use the target-compatible GDB.')
    mi.command('-gdb-set architecture auto')
    mi.command('-gdb-set pagination off')
    mi.command('-gdb-set print frame-arguments none')
    threads = mi.command('-thread-info')
    if threads is None:
      raise RuntimeError('Thread information unavailable')
    current = str(threads.get('current-thread-id', '1'))
    firmware = args.elf.read_bytes()
    tasks = []
    for thread in threads.get('threads', []):
      thread_id = str(thread['id'])
      match = re.fullmatch(r'process (0x[0-9a-fA-F]+|[0-9]+)', thread.get('target-id', ''))
      tcb = int(match.group(1), 0) if match else None
      fields = {}
      if tcb is not None:
        for field in ('uxPriority', 'uxBasePriority', 'pxStack', 'pxTopOfStack', 'pxEndOfStack'):
          fields[field] = mi.number(f'(unsigned long)((TCB_t*){tcb})->{field}')
      tasks.append({'id': thread_id, 'tcb': tcb, 'name': task_name(mi, tcb, firmware) if tcb else 'unknown',
                    'fields': fields, 'frames': stack_frames(mi, thread_id)})
      if tcb == crashed_tcb:
        current = thread_id
    mi.command(f'-thread-select {current}')
    current_task = next(task for task in tasks if task['id'] == current)
    metrics = crash_metrics(mi, dump.core_elf)
    build = build_info(mi, dump.core_elf)
    register_names = mi.command('-data-list-register-names') or {}
    registers = mi.command('-data-list-register-values x') or {}
    lines = ['OTGateway ESP32 core dump report', '', f'Target: {target} ({"Xtensa" if xtensa else "RISC-V"})',
             f'Dump: {args.dump.resolve()}',
             f'Firmware ELF: {args.elf.resolve()}', f'Dump size: {args.dump.stat().st_size} bytes',
             'Decoder: installed esp_coredump + GDB/MI; no device access',
             'ROM symbols: ' + ('available' if rom is not None else 'not available; ROM frames may lack symbols'),
             '', 'BUILD INFORMATION']
    lines += render_build_info(build) + ['', 'PANIC CAUSE']
    abort_explanation = ('IllegalInstruction is the intentional trap used by this abort call.' if xtensa
                         else 'The abort deliberately stopped execution; the symbolic call stack identifies the call site.')
    if known_reason:
      lines += [reason, 'Cause: deliberate esp_system_abort() call; the saved caller frames identify the call site.',
                f'Uptime at the test call: {known_reason.group(1)} us']
      origin = next((frame for frame in current_task['frames']
                     if (frame.get('fullname') or frame.get('file', '')).replace('\\', '/').endswith('/system.cpp')
                     and frame.get('line')), None)
      if origin:
        lines.append(f"Application call site: {origin.get('fullname') or origin['file']}:{origin['line']}")
      lines.append(abort_explanation)
    elif low_heap:
      lines += ['OTGateway: low internal heap',
                'Cause: low-heap protection deliberately called esp_system_abort() in the heap-monitoring task.',
                'The protection detected less than 2048 bytes of free internal heap or largest free block.',
                ('The cached heap snapshot below can predate the low-heap guard; its values are not panic-time measurements.'
                 if isinstance(metrics['heap'], dict) else
                 'Largest free block is not captured; the low-heap guard can also trigger on a small largest block.'),
                abort_explanation]
    elif startup_failure:
      lines += [reason, 'Cause: task manager startup failed; the application deliberately called esp_system_abort().',
                abort_explanation]
    else:
      lines += ['Panic text not captured or outside the known diagnostic formats; arbitrary text omitted.',
                'Use the exception registers and symbolic call stack below for the cause.']
    lines += [f"Crashed thread: {current}; task: {current_task['name']}; TCB: {crashed_tcb:#x}" if crashed_tcb
              else f'Crashed thread: {current}', '', 'ADDITIONAL CRASH METRICS (saved in the core dump)']
    lines += render_metrics(metrics) + ['', 'EXCEPTION REGISTERS']
    if xtensa and len(words) >= 5:
      exception_output = io.StringIO()
      with contextlib.redirect_stdout(exception_output):
        print_exc_regs_info(words)
      lines += exception_output.getvalue().splitlines()
    elif not xtensa:
      lines.append('RISC-V extra exception details are not decoded here; see the numeric GDB registers below.')
    else:
      lines.append('Xtensa exception registers were not captured.')
    lines += ['', 'CRASHED THREAD REGISTERS']
    names = register_names.get('register-names', [])
    for register in registers.get('register-values', []):
      value = register.get('value', '')
      index = int(register['number'])
      if re.fullmatch(r'0x[0-9a-fA-F]+', value) and index < len(names) and names[index]:
        lines.append(f'{names[index]:16} {value}')
    lines += ['', 'COMPLETE CRASHED THREAD STACK (all frames returned by GDB; arguments omitted)']
    lines += [render_frame(frame) for frame in current_task['frames']]
    if known_reason:
      for frame in current_task['frames']:
        source = (frame.get('fullname') or frame.get('file', '')).replace('\\', '/')
        if source.endswith('/system.cpp'):
          mi.command(f"-stack-select-frame {frame['level']}")
          lines += ['', 'SELECTED TEST FRAME VALUES']
          for name in ('diagnosticMarker', 'crashUptimeUs'):
            value = mi.number(f'(unsigned long long){name}')
            rendered = f'{value:#x} ({value})' if value is not None else 'not available in this frame'
            lines.append(f'{name}: {rendered}')
          break
    lines += ['', 'TEST CALL SOURCE CONTEXT'] + code_context(current_task['frames'], args.source_map)
    lines += ['', f'ALL SAVED TASKS ({len(tasks)})',
              'Stack used/free are byte estimates from saved TCB pointers, as in esp-coredump; not high-water marks.',
              'The running task TCB pxTopOfStack may precede the fault; its estimate is not an exact panic-time measurement.',
              'No frame count limit is applied. Unwinding stops where GDB cannot recover further frames.',
              'Missing source locations are reported explicitly; missing DWARF cannot be reconstructed.']
    for task in tasks:
      fields = task['fields']
      tcb_text = f"0x{task['tcb']:08x}" if task['tcb'] is not None else 'unknown'
      lines += ['', f"Thread {task['id']}: {task['name']}; TCB {tcb_text}" + (' [CRASHED]' if task['id'] == current else '')]
      lines.append(f"Priority current/base: {fields.get('uxPriority')}/{fields.get('uxBasePriority')}")
      start, top, end = (fields.get(key) for key in ('pxStack', 'pxTopOfStack', 'pxEndOfStack'))
      if all(value is not None for value in (start, top, end)):
        lines += [f'Stack start/top/end: {start:#x}/{top:#x}/{end:#x}',
                  f'Stack used/free estimate: {abs(end - top)}/{abs(top - start)} bytes']
      else:
        lines.append('Stack bounds/used/free: not captured')
      lines += [render_frame(frame) for frame in task['frames']]
      if not task['frames']:
        lines.append('Stack unavailable to GDB.')
    lines += ['', 'Runtime arguments, arbitrary local variables and memory strings are omitted.']
    return '\n'.join(lines) + '\n', {
      'tasks': len(tasks), 'current_frames': len(current_task['frames']),
      'all_frames': sum(len(task['frames']) for task in tasks),
      'source_frames': sum(bool(frame.get('line')) for task in tasks for frame in task['frames']),
      'report_lines': len(lines),
      'heap_captured': (metrics['heap']['total_free_bytes'] is not None if isinstance(metrics['heap'], dict)
                        else metrics['heap'] is not None)}
  finally:
    if mi is not None:
      mi.close()
    for path in temporary:
      Path(path).unlink(missing_ok=True)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('dump', type=Path)
  parser.add_argument('elf', type=Path)
  parser.add_argument('output', type=Path, nargs='?', help='Report file; omit to print the report to stdout')
  parser.add_argument('--stdout', action='store_true', help='Print the report, also when an output file is supplied')
  parser.add_argument('--source-map', type=Path, help='Directory of matching source snapshots for the test call context')
  parser.add_argument('--platformio-home', type=Path, default=Path.home() / '.platformio')
  parser.add_argument('--chip', choices=['auto', *SUPPORTED_TARGETS], default='auto',
                      help='Verify the dump target; default: detect from the native core dump header')
  parser.add_argument('--gdb', type=Path)
  parser.add_argument('--rom', type=Path)
  args = parser.parse_args()
  if not args.dump.is_file():
    parser.error('Core dump file not found; supply an existing native raw core dump.')
  if not args.elf.is_file():
    parser.error('Firmware ELF not found; supply the exact ELF from the crashed firmware build.')
  if args.gdb is not None and not args.gdb.is_file():
    parser.error('GDB executable not found; use --gdb or --platformio-home to select the installed PlatformIO tools.')
  if args.rom is not None and not args.rom.is_file():
    parser.error('ROM ELF not found; use --rom to select an existing target-compatible ROM ELF.')
  # Decoder/GDB diagnostics can contain arbitrary target data: keep them in memory.
  try:
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
      text, result = report(args)
    if args.output is not None:
      args.output.parent.mkdir(parents=True, exist_ok=True)
      args.output.write_text(text, encoding='utf-8')
    if args.stdout or args.output is None:
      print(text, end='')
    else:
      print(json.dumps(result))
  except Exception as error:
    if isinstance(error, AnalysisError):
      print(str(error), file=sys.stderr)
      return 1
    frame = traceback.extract_tb(error.__traceback__)[-1]
    print(json.dumps({'error_type': type(error).__name__,
                      'error_location': f'{Path(frame.filename).name}:{frame.lineno}'}), file=sys.stderr)
    return 1
  return 0


if __name__ == '__main__':
  raise SystemExit(main())
