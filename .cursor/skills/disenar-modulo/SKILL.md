---
name: disenar-modulo
description: Guía el diseño de módulos, drivers y clases en C++ del proyecto (ESP32 y Linux). Usar al crear, partir o reorganizar un driver, una clase, un .hpp/.cpp o la separación entre lógica y hardware.
---

# Diseñar un módulo

## Por qué existe

Cada módulo se va a explicar en la defensa. Un buen módulo se entiende leyendo solo su `.hpp`
y se explica en dos minutos. El objetivo es orden y claridad, no flexibilidad futura.

## Principios

- Una responsabilidad por módulo. El nombre la dice sin ambigüedad (`StepperDriver`, no `MotorManager`).
- El `.hpp` es la interfaz: pocos métodos públicos, con nombres que digan qué hacen.
  Los detalles quedan privados o en el `.cpp`.
- Los drivers no saben nada de la lógica: no conocen estados, comandos ni protocolo.
- La lógica no conoce pines ni periféricos: le habla a los drivers.
- El constructor solo guarda configuración; el hardware se inicializa en `init()`.
- Sin herencia ni polimorfismo salvo que GuiLite lo exija. Composición simple.
- Si un módulo necesita mucho de otro, quizás la frontera está mal puesta: decilo.

## Cómo se ve un buen resultado

```cpp
// Driver del motor paso a paso 28BYJ-48 con placa ULN2003.
// Solo sabe dar pasos y apagar las bobinas; la velocidad la decide quien lo llama.
class StepperDriver {
public:
    StepperDriver(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4);
    void init();          // configura los 4 GPIO como salida
    void step();          // avanza medio paso
    void release();       // apaga las bobinas para que no calienten
private:
    std::array<gpio_num_t, 4> pins_;
    int phase_ = 0;
};
```

Corto, sin dependencias de la lógica y con un comentario que explica el porqué.

## Señales de que algo está mal

- Una clase con más de ~7 métodos públicos o un `.cpp` de cientos de líneas.
- Un driver que incluye headers de la lógica, o lógica que incluye `driver/gpio.h`.
- Abstracciones con una sola implementación ("por si cambiamos el motor").
- Getters y setters para todo en vez de métodos con intención.
