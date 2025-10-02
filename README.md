# Parcial2
Sistema de chat con colas de mensajes (System V)
Archivos:
 - servidor.c
 - cliente.c

Compilación:
 gcc -o servidor servidor.c -Wall
 gcc -o cliente cliente.c -pthread -Wall

Ejecución:
 1) En una terminal: ./servidor
 2) Abrir N terminales para clientes:
    ./cliente Maria
    ./cliente Juan
    ./cliente Camila
 3) En cada cliente:
    > join General
    (el servidor confirma)
    > Hola a todos
    (los demás usuarios en la sala verán el mensaje)

Comandos del cliente:
 - join <sala>   : unirse (o crear si no existe) a una sala
 - leave         : salir de la sala actual
 - <texto>       : escribir cualquier otra línea la envía como mensaje a la sala actual
 - CTRL+C        : salir (se enviará LEAVE al servidor si estás en una sala)

Notas de diseño:
 - Cada cliente crea una cola privada y pasa su qid al servidor en JOIN; el servidor reenvía a cada qid.
 - Esto evita que múltiples clientes "consuman" el mismo mensaje de una cola común (problema de usar una sola cola por sala).
 - El servidor elimina la cola global al recibir SIGINT.
