# USB BOOTEABLE CON PROGRAMA PROPIO

---

## 1. EXPLICACIÓN

El grupo va a construir un programa que prepare una memoria USB para ejecutar **un programa de arranque propio**, sin cargar un sistema operativo existente. Al seleccionar la USB en el menú de arranque de un equipo compatible, debe aparecer el resultado del programa desarrollado por el grupo, como por ejemplo un mensaje.

El proyecto tiene dos partes: **el programa que arranca y la herramienta que prepara la USB**. La herramienta usa llamadas al sistema (syscalls) o las API del sistema operativo para trabajar con la memoria USB. El programa de arranque usa los servicios de BIOS o UEFI, o accede al hardware directamente; en ese momento no está ejecutándose el sistema operativo desde el que se preparó la USB.

---

## 2. QUÉ DEBE HACER

1. **Programa de arranque propio:** ejecutar código del grupo con un resultado visible e identificable, sin depender de un sistema operativo instalado en la USB.
2. **Tipo de arranque:** preparar el medio para BIOS o UEFI. El programa y su formato deben ser compatibles con el equipo donde se probará.
3. **Construcción del medio:** generar la imagen o estructura de arranque necesaria para que el firmware localice y ejecute el programa.
4. **Preparador propio:** seleccionar la USB y crear en ella las estructuras, formato y contenido necesarios para la ruta elegida.
5. **Identificación del destino:** mostrar identificador, capacidad y datos disponibles del dispositivo; verificar que es la USB seleccionada antes de modificarla.
6. **Confirmación de sobrescritura:** advertir que se perderá el contenido anterior y solicitar confirmación explícita.
7. **Progreso y resultado:** mostrar el avance de la preparación, completar las escrituras pendientes y comprobar que los datos y el formato de arranque quedaron correctamente guardados.
8. **Manejo de errores:** comunicar falta de permisos, destino no válido, desconexión, capacidad insuficiente y fallos de escritura o verificación sin declarar éxito.
9. **Prueba virtual:** ejecutar el programa de arranque en una máquina virtual o emulador compatible antes de probar la USB física.
10. **Arranque físico:** iniciar desde la USB en un equipo compatible y mostrar el resultado del programa propio.

---

## 3. REGLAS DE IMPLEMENTACIÓN

- **Código de arranque propio.** El comportamiento visible debe provenir del programa desarrollado por el grupo, no de una imagen de sistema operativo ya preparada.
- **Preparación propia.** La herramienta debe realizar las operaciones de preparación del dispositivo; no se utiliza un grabador externo para sustituirla.
- **Syscalls y firmware.** Distinguir las llamadas al sistema usadas por la herramienta de preparación de los servicios BIOS o UEFI usados durante el arranque.
- **Formato coherente.** La USB debe contener las estructuras de arranque que requiere la ruta elegida. Formatear una unidad o copiar un ejecutable común no basta para hacerla booteable.
- **Compatibilidad comprobada.** El equipo de prueba debe admitir el tipo de arranque elegido. Si se usa UEFI, se debe tener en cuenta Secure Boot, que puede impedir ejecutar código no autorizado. No se cambian claves ni opciones de seguridad de equipos ajenos.
- **Destino seguro.** Usar una USB dedicada al laboratorio y respaldar los datos necesarios. La herramienta debe rechazar discos del sistema o dispositivos cuya identidad no pueda comprobar y solicitar confirmación antes de escribir.
- **Escritura completa.** Comprobar que se escribieron todos los datos, atender los errores y cerrar los archivos y dispositivos. Una preparación interrumpida no se presenta como verificada.
- **Programa no destructivo.** El código que arranca debe limitarse a su demostración visible sin modificar los discos internos del equipo.
