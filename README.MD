# Implementación de Controladores de Dispositivos
## Trabajo final de la asignatura

En este respositorio se encuentran los distintos archivos fuentes desarrollados para el trabajo final de asignatura. El contenido del mismo es el siguiente:

1. En la carpeta `dts` se encuentra el archivo principal de Device Tree, el cual se modificó para incluir dos dispositivos: el chip MCP7940N que provee un RTC ubicado en la placa principal y el chip esclavo de la placa de expansión.

2. En la carpeta `qxw_ioe_driver` se encuentra el codigo fuente del modulo de kernel que implementa el driver para la placa de expansion con su correspondiente makefile.

3. En la carpeta `scripts` se encuentran el script que implementa el servicio de puesta en hora a partir del RTC durante el inicio del sistema y el script de prueba que activa una salida para una determinada tarjeta de RFID.