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

  Equitherm() = default;

  // kn, kk, kt
  Equitherm(float new_kn, float new_kk, float new_kt) {
    Kn = new_kn;
    Kk = new_kk;
    Kt = new_kt;
  }

  // лимит выходной величины
  void setLimits(unsigned short min_output, unsigned short max_output) {
    _minOut = min_output;
    _maxOut = max_output;
  }

  // возвращает новое значение при вызове
  datatype getResult() {
    datatype output = getResultN() + getResultK() + getResultT();
    output = constrain(output, _minOut, _maxOut);		// ограничиваем выход
    return output;
  }

private:
  unsigned short _minOut = 20, _maxOut = 90;

  // температура контура отопления в зависимости от наружной температуры
  datatype getResultN() {
    float a = (-0.21 * Kn) - 0.06;     // a = -0,21k — 0,06
    float b = (6.04 * Kn) + 1.98;      // b = 6,04k + 1,98
    float c = (-5.06 * Kn) + 18.06;    // с = -5,06k + 18,06
    float x = (-0.2 * outdoorTemp) + 5; // x = -0.2*t1 + 5
    return (a * x * x) + (b * x) + c;       // Tn = ax2 + bx + c
  }

  // поправка на желаемую комнатную температуру
  datatype getResultK() {
    return (targetTemp - 20) * Kk;
  }

  // Расчет поправки (ошибки) термостата
  datatype getResultT() {
    return constrain((targetTemp - indoorTemp), -3, 3) * Kt;
  }
};