---
name: revisar-defensa
description: Revisa el código con la mirada del docente que evalúa la defensa del TP. Usar solo cuando el usuario lo invoque con /revisar-defensa.
disable-model-invocation: true
---

# Revisar para la defensa

## Por qué existe

En la defensa el docente hace preguntas sobre el código y pide modificaciones en vivo.
Esta revisión anticipa eso antes de que pase. No cambia código: informa.

## Qué mirar

- Lo que cuesta explicar: partes rebuscadas, nombres confusos, saltos difíciles de seguir.
- Decisiones sin justificar: prioridades de tasks, tamaños de stack, timeouts, elección de IPC.
- Temas de la materia que el código muestra o que faltan: FreeRTOS, IPC, framebuffer,
  acceso a hardware, scripts y opciones de compilación, manejo de memoria y filesystem.
- Modificaciones en vivo probables (cambiar una prioridad, agregar un botón, cambiar el baudrate,
  agregar un estado) y si serían fáciles o requieren tocar muchos lugares.
- Riesgos de que falle en la demo: errores sin chequear, condiciones de carrera, dependencias de la PC.

## Cómo se ve un buen resultado

Una respuesta corta con tres partes:

1. **Hallazgos**: lista priorizada de lo que conviene arreglar, con archivo y motivo.
2. **Preguntas probables del docente**: 5 a 10, cada una con dónde está la respuesta en el código.
3. **Modificaciones en vivo**: 3 a 5 probables, con qué archivos habría que tocar.

## Señales de que la revisión está mal hecha

- Sugerir reescrituras grandes o patrones más sofisticados.
- Comentarios de estilo menores mezclados con problemas reales.
- Modificar archivos en vez de informar.
