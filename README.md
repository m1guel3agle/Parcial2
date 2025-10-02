# Parcial2
Sistema de chat con colas de mensajes 
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
 

