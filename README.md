# Dub Echo

Eco de cinta estilo dub para tocar en vivo con rekordbox, pensado para un envío paralelo del mixer:
la música seca nunca pasa por la computadora, y Dub Echo devuelve **sólo el eco** a un canal propio.

- Repeticiones que se oscurecen y saturan como cinta.
- Feedback hasta 110%: autooscilación controlada, sin clip digital.
- Cambiar el tiempo dobla el pitch (inercia de cinta), el gesto clásico de dub.
- Throw, freeze, kill y tap tempo con divisiones (1/8, 1/8·, 1/4, 1/4·, 1/4T).
- Control desde el KORG nanoPAD2 (pad XY = tiempo y feedback) y desde el teclado.
- Corre en macOS 10.13 o superior (incluye Catalina en Mac Intel 2012) y en Apple Silicon.

## Conexión

```
FLX4 master (RCA) ──┬──► canal del mixer (música seca)
                    │
                (cable Y)
                    │
                    └──► UCA222 entrada ─► Dub Echo ─► UCA222 salida ─► canal libre del mixer (sólo eco)
```

- **Puerto USB 1:** la FLX4, siempre directa.
- **Puerto USB 2:** hub alimentado con la UCA222 y el nanoPAD2.
  Si el hub falla, se pierde el eco, nunca la música.

Si la UCA222 se desconecta, Dub Echo silencia su salida (no cae a los parlantes ni al micrófono
de la Mac) y la vuelve a tomar sola al reconectarla. Lo mismo con el nanoPAD2.

## Instalar

1. Descargá `DubEcho.dmg`: desde **Releases** del repositorio, o desde la pestaña **Actions**
   (última ejecución verde → sección *Artifacts* → `DubEcho-macOS`, viene en un .zip con el .dmg adentro).
2. Abrí el .dmg y arrastrá **Dub Echo** a **Aplicaciones**.
3. La primera vez: **clic derecho → Abrir → Abrir** (no está firmada por Apple).
4. Aceptá el permiso de micrófono: es la entrada de la placa.

## Primera configuración

1. **Audio / MIDI** → salida y entrada: `USB Audio CODEC` (la UCA222). Si está conectada al abrir
   la app por primera vez, ya la elige sola. Buffer: 512 está bien (la latencia no importa en un eco).
2. En la misma ventana, activá el **nanoPAD2** en *Active MIDI inputs* (también se activa solo).
   **No actives la FLX4** acá: rekordbox ya la usa.
3. Probá con el fader del canal de retorno abajo, mandá un throw y subilo de a poco.

## Controles

| Acción | Teclado | nanoPAD2 (por defecto) |
|---|---|---|
| Tiempo | ← → (shift = fino) | Pad XY, eje X (pitch bend) |
| Feedback | ↑ ↓ (shift = fino) | Pad XY, eje Y (CC 1) |
| Throw (mantener: entra música al eco) | Espacio | Pad nota 36 |
| Tap tempo | T | Pad nota 37 |
| Kill (corta todas las repeticiones) | K | Pad nota 38 |
| Freeze (mantener: loop congelado) | F | Pad nota 39 |
| Envío continuo on/off | E | Pad nota 40 |
| Divisiones 1/8 · 1/8· · 1/4 · 1/4· · 1/4T | 1 a 5 | Pads notas 41 a 45 |

Si tu nanoPAD2 manda otras notas o CC, **Mapeo MIDI** → *Aprender* → tocá el pad. Ahí también se pueden
asignar las perillas, y abajo se ve el último mensaje MIDI recibido.

Al soltar el pad XY, el nanoPAD2 vuelve al centro/cero: Dub Echo ignora ese salto, así el tiempo y el
feedback quedan donde los dejaste.

**El teclado sólo funciona con la ventana de Dub Echo al frente.** Si hacés clic en rekordbox, el
teclado pasa a rekordbox. El nanoPAD2 funciona siempre, esté al frente la ventana que esté.

## Perillas

- **Inercia:** cuánto tarda la "cinta" en llegar al tiempo nuevo. Más inercia = glides de pitch más largos.
- **Wow / flutter:** inestabilidad de la cinta (variaciones lentas y rápidas de velocidad).
- **Saturación:** cuánto se aplasta la señal en la cabeza de grabación. También define a qué volumen se
  estabiliza la autooscilación.
- **Corte graves / agudos:** filtros dentro del loop: cada repetición pierde más graves y agudos.
  Clásico de dub: graves altos (150–300 Hz) para que las repeticiones no embarren el bajo.
- **Entrada / salida:** niveles. Preferí manejar el volumen del eco con el fader del mixer.

El pad XY marca las divisiones del tempo actual. La línea verde muestra dónde está la cinta realmente
mientras hace el glide.

## Uso en vivo

- Dejá **Envío** apagado y usá **Throw** en los momentos puntuales: suena mucho más a dub que un eco
  constante.
- Feedback por encima de 100% (zona roja) se sostiene solo: bajalo o tocá **Kill** para cortarlo.
- **Freeze** + mover el tiempo = sirena de cinta improvisada.
- Probá el hub alimentado en casa un par de horas antes de llevarlo a una fecha.

## Compilar

GitHub compila solo en cada push (`.github/workflows/build-macos.yml`) y para publicar un Release
alcanza con crear un tag `v0.1.0`, `v0.2.0`, etc.

En una Mac con Xcode y CMake:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
```

Pruebas del motor de eco (no necesitan JUCE):

```
c++ -O2 -std=c++17 -ISource tests/render_test.cpp -o render_test && ./render_test .
```

Estructura: `Source/TapeEchoDSP.h` es el motor de eco puro; `DubEchoProcessor` lo envuelve como
procesador de JUCE (sirve tal cual para un plugin AU a futuro); `MainComponent` maneja audio, MIDI e interfaz.
