---
name: disenar-pantalla
description: Guía el diseño de pantallas y menús de la HMI con GuiLite. Usar al crear o modificar una pantalla, widgets, navegación arriba/abajo/OK o el cambio entre pantallas.
---

# Diseñar una pantalla de la HMI

## Por qué existe

La HMI tiene que entenderse de un vistazo y manejarse con tres entradas: arriba, abajo y OK.
Es una interfaz de operario, no una app: clara, ordenada y sin adornos.

## Principios

- Pocas pantallas y pocos elementos por pantalla. Mejor dos pantallas claras que cinco cargadas.
- Arriba la información (estado, valores), abajo las acciones (botones en columna).
- El orden del foco sigue el orden visual: abajo = siguiente, arriba = anterior.
- El elemento con foco se distingue sin dudas.
- Estilo industrial sobrio: fondo neutro, texto legible, color solo para lo anormal (EMERGENCIA en rojo).
- Cada pantalla se arma en su propia función; los callbacks son cortos y llaman a algo con nombre.
- La vista no decide reglas de la máquina: muestra el estado que llega y manda comandos.
- Usar componentes distintos de GuiLite cuando aporten (la rúbrica los cuenta), nunca por relleno.

## Cómo se ve un buen resultado

```
┌──────────────────────────────┐
│ CINTA L1                     │
│ Estado:    MARCHA            │
│ Velocidad: MEDIA             │
│ Piezas:    27                │
├──────────────────────────────┤
│   ▐ ARRANCAR ▌               │
│   [ PARAR    ]               │
│   velocidad  [−] 2 [+]       │
└──────────────────────────────┘
```

Es una referencia de estilo, no una plantilla obligatoria.

## Señales de que algo está mal

- Elementos que no se pueden alcanzar con arriba/abajo, o un orden de foco que salta.
- Coordenadas mágicas repetidas por todos lados en vez de unas pocas constantes de layout.
- Lógica de la cinta (estados, reglas) dentro de los callbacks de la vista.
- Colores fuertes o textos que compiten por atención sin motivo.
