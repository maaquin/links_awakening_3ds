# The Legend of Zelda: Link's Awakening 3DS

Una reinterpretación de **The Legend of Zelda: Link's Awakening DX** para Nintendo 3DS, basada en el proyecto [Link's Awakening Portable](https://github.com/sp00nznet/linksawakening-portable).

El objetivo no es realizar un simple port, sino aprovechar las características de Nintendo 3DS para crear una experiencia diferente, manteniendo la esencia y estética del juego original.

---

## 🎮 Idea

El proyecto busca adaptar Link's Awakening DX a las posibilidades del Nintendo 3DS mediante varias modificaciones principales:

- 🌎 **Mundo continuo:** eliminar progresivamente la estructura original basada en pantallas independientes.
- 🕶️ **3D estereoscópico:** utilizar los sprites 2D originales colocados sobre planos 3D para crear profundidad.
- 📱 **Segunda pantalla:** utilizar la pantalla inferior para inventario, equipamiento, mapas y otros elementos de interfaz.
- 🎮 **Controles ampliados:** aprovechar los botones adicionales del 3DS para permitir más objetos equipados simultáneamente.
- ✏️ **Interfaz táctil:** aprovechar la pantalla táctil para interactuar con el inventario y el equipamiento.

La intención es explorar cómo podría sentirse Link's Awakening si hubiese sido diseñado específicamente para Nintendo 3DS.

---

## 🛠️ Tecnologías

- C
- devkitPro / devkitARM
- libctru
- Nintendo 3DS Homebrew
- Static Recompilation
- `gb-recompiled`

El proyecto utiliza como base el runtime de Game Boy y la recompilación estática proporcionada por los proyectos originales.

---

## 📌 Estado

Actualmente el proyecto se encuentra en desarrollo temprano.

- [x] Compilación para Nintendo 3DS
- [x] Ejecución en hardware real
- [x] Carga de ROM desde la SD
- [ ] Interfaz de segunda pantalla
- [ ] Sistema de inventario/equipamiento
- [ ] Mundo continuo
- [ ] 3D estereoscópico
- [ ] Pulido y optimización

---

## 🙏 Créditos

Este proyecto se basa principalmente en:

- **[sp00nznet/linksawakening-portable](https://github.com/sp00nznet/linksawakening-portable)** — base principal del port para Nintendo 3DS.

- **[arcanite24/gb-recompiled](https://github.com/arcanite24/gb-recompiled)** — recompilador estático de Game Boy utilizado como base.

- **[sp00nznet/gb-recompiled](https://github.com/sp00nznet/gb-recompiled)** — fork del runtime utilizado por `linksawakening-portable`.

- **[sp00nznet/LinksAwakening](https://github.com/sp00nznet/LinksAwakening)** — proyecto upstream de recompilación de Link's Awakening DX.

### Juego original

*The Legend of Zelda: Link's Awakening DX*  
© 1993, 1998 Nintendo / Grezzo.

No afiliado ni respaldado por Nintendo.

Este repositorio no distribuye la ROM original del juego.

---

## 📚 Inspiración

Algunas ideas del proyecto están inspiradas o toman como referencia otros proyectos relacionados con Zelda y Nintendo 3DS:

- **[Link's Awakening DX HD](https://github.com/ladxhd/projectz)** — referencia para el mundo continuo, comportamiento de objetos y otros aspectos de la experiencia.

- **[A Link to the Past 3DS](https://github.com/EstebanPdN/zelda-alttp-3ds)** — referencia para la adaptación a Nintendo 3DS, segunda pantalla y uso de ROM externa. :contentReference[oaicite:1]{index=1}

- **[The Minish Cap 3DS](https://github.com/EstebanPdN/zelda-tmc-3ds)** — referencia para la interfaz de segunda pantalla, controles táctiles y otras características específicas de Nintendo 3DS. :contentReference[oaicite:2]{index=2}

- **[Pokémon Emerald 3Ds Dual Screen](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen)** — inspiración para el uso de dos pantallas, controles táctiles y la presentación de elementos 2D mediante el hardware 3D del Nintendo 3DS. :contentReference[oaicite:3]{index=3}

Estos proyectos se utilizan como referencia e inspiración; no forman parte de este código salvo donde se indique expresamente.

---

## 📌 Autor

- **Luciano Maquin** — [@Maaquin](https://github.com/Maaquin)