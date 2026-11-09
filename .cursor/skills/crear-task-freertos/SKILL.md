---
name: crear-task-freertos
description: Guía la creación y modificación de tasks, ISRs, colas, mutex y semáforos de FreeRTOS en el firmware de la ESP32. Usar al agregar concurrencia, cambiar prioridades o comunicar tasks entre sí.
---

# Crear tasks y mecanismos de FreeRTOS

## Por qué existe

FreeRTOS es lo que se evalúa en el firmware. Cada task, prioridad y mecanismo de sincronización
tiene que tener una razón que se pueda decir en voz alta en la defensa.

## Principios

- Cada task tiene un único trabajo y una razón de ser. Si dos tasks hacen lo mismo, sobra una.
- La prioridad se justifica por urgencia: seguridad > control del motor > entradas > comunicación.
- Elegir con criterio, y dejarlo dicho en un comentario:
  - periódica (`vTaskDelayUntil`) si el período importa; por evento si espera algo.
  - cola para pasar datos o comandos entre tasks.
  - mutex para proteger datos compartidos.
  - semáforo binario o notificación para que una ISR despierte a una task.
- Las ISR solo avisan. Nada de lógica, logs ni esperas dentro de una ISR; usar las funciones `...FromISR`.
- FreeRTOS se llama directo, sin envoltorios. La creación de todas las tasks queda junta en un solo lugar.
- Timeouts explícitos: decidir si se espera para siempre (`portMAX_DELAY`), un tiempo, o nada.

## Cómo se ve un buen resultado

```cpp
// Motor: periódica; si se atrasa se pierden pasos. Prioridad alta, debajo de seguridad.
xTaskCreatePinnedToCore(motorTask, "motor", 3072, &stepper, 4, nullptr, 1);
```

Una línea por task y un comentario que responde "¿por qué esta prioridad?".

## Señales de que algo está mal

- Una task sin comentario de prioridad, o dos tasks con la misma prioridad sin motivo.
- Variables compartidas entre tasks sin mutex ni cola.
- `vTaskDelay` usado para "esperar a que otra task termine".
- Un `xSemaphoreTake` con varios `return` antes del `Give`.
- Stacks copiados de ejemplos de otros micros (en ESP-IDF el stack va en bytes).
