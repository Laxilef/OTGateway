#include <Arduino.h>

#if defined(EQUITHERM_INTEGER)
// расчёты с целыми числами
typedef int datatype;
#else
// расчёты с float числами
typedef float datatype;
#endif

class Equitherm {
public:
  datatype targetTemp = 0;
  datatype indoorTemp = 0;
  datatype outdoorTemp = 0;
  float Kn = 0.0;
  float Kk = 0.0;
  float Kt = 0.0;
  float Ke = 1.3;

  Equitherm() = default;

  // kn, kk, kt, Ke
  Equitherm(float new_kn, float new_kk, float new_kt, float new_ke) {
    Kn = new_kn;
    Kk = new_kk;
    Kt = new_kt;
    Ke = new_ke;
  }

  // лимит выходной величины
  void setLimits(unsigned short min_output, unsigned short max_output) {
    _minOut = min_output;
    _maxOut = max_output;
  }

  // возвращает новое значение при вызове
  datatype getResult() {
    datatype output = getResultN() + Kk + getResultT();
    output = constrain(output, _minOut, _maxOut);		// ограничиваем выход
    return output;
  }

private:
  unsigned short _minOut = 20, _maxOut = 90;

  datatype getResultN() {
    float tempDiff  = targetTemp - outdoorTemp,
          maxPoint = targetTemp - (_maxOut - targetTemp) / Kn,
          sf        = (_maxOut - targetTemp) / pow(targetTemp - maxPoint, 1.0 / Ke),
          T_rad     = targetTemp + sf * pow(tempDiff, 1.0 / Ke);
    return T_rad > _maxOut ? _maxOut : T_rad;
}

  // Расчет поправки (ошибки) термостата
  datatype getResultT() {
    return constrain((targetTemp - indoorTemp), -3, 3) * Kt;
  }
};
