#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

// ------------------- Constantes -------------------
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Tipos de mensajes
#define T_JOIN 1
#define T_RESP 2
#define T_MSG 3
#define T_LEAVE 4

// Estructura de mensaje
struct mensaje {
    long mtype;
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
    int qid_cliente;
};

// ------------------- Variables globales -------------------
int cola_global = -1;           // Cola global (del servidor)
int cola_privada = -1;          // Cola privada del cliente
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";
volatile int running = 1;       // Flag de control del cliente

// ------------------- Funciones -------------------

// Limpia recursos y avisa al servidor al cerrar el cliente
void cleanup_and_exit(int signo) {
    (void)signo; // evitar warning
    if (cola_privada != -1 && strlen(sala_actual) > 0) {
        struct mensaje m;
        m.mtype = T_LEAVE;
        strncpy(m.remitente, nombre_usuario, MAX_NOMBRE-1);
        strncpy(m.sala, sala_actual, MAX_NOMBRE-1);
        m.qid_cliente = cola_privada;
        msgsnd(cola_global, &m, sizeof(struct mensaje) - sizeof(long), 0);
    }
    if (cola_privada != -1) {
        msgctl(cola_privada, IPC_RMID, NULL);
    }
    printf("\nCliente terminado.\n");
    exit(0);
}

// Hilo que se encarga de escuchar la cola privada del cliente
void *recibir_mensajes(void *arg) {
    (void)arg; // evitar warning
    struct mensaje m;
    while (running) {
        if (cola_privada != -1) {
            // Recibir cualquier mensaje dirigido a este cliente
            if (msgrcv(cola_privada, &m, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
                usleep(100000);
                continue;
            }
            // Mostrar mensajes
            if (m.mtype == T_MSG) {
                printf("\n%s: %s\n> ", m.remitente, m.texto);
                fflush(stdout);
            } else if (m.mtype == T_RESP) {
                printf("\n[SERVIDOR] %s\n> ", m.texto);
                fflush(stdout);
            }
        } else {
            usleep(100000);
        }
    }
    return NULL;
}

// ------------------- Función principal -------------------
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }

    // Guardar nombre de usuario
    strncpy(nombre_usuario, argv[1], MAX_NOMBRE-1);

    // Capturar señal Ctrl+C para cerrar bien
    signal(SIGINT, cleanup_and_exit);

    // Conectarse a la cola global
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global (¿servidor corriendo?)");
        exit(1);
    }

    // Crear cola privada para recibir mensajes
    cola_privada = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (cola_privada == -1) {
        perror("Error al crear cola privada");
        exit(1);
    }

    printf("Bienvenido, %s. Usa 'join <sala>' para entrar.\n", nombre_usuario);

    // Lanzar hilo para escuchar mensajes entrantes
    pthread_t hilo;
    pthread_create(&hilo, NULL, recibir_mensajes, NULL);

    char linea[MAX_TEXTO];
    struct mensaje m;

    // Bucle principal: leer comandos del usuario
    while (1) {
        printf("> ");
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            cleanup_and_exit(0);
            break;
        }
        linea[strcspn(linea, "\n")] = '\0'; // Quitar salto de línea

        // Comando join
        if (strncmp(linea, "join ", 5) == 0) {
            char sala[MAX_NOMBRE];
            if (sscanf(linea+5, "%s", sala) != 1) {
                printf("Uso: join <sala>\n");
                continue;
            }
            memset(&m,0,sizeof(m));
            m.mtype = T_JOIN;
            strncpy(m.remitente, nombre_usuario, MAX_NOMBRE-1);
            strncpy(m.sala, sala, MAX_NOMBRE-1);
            m.qid_cliente = cola_privada;
            msgsnd(cola_global, &m, sizeof(struct mensaje) - sizeof(long), 0);
            strncpy(sala_actual, sala, MAX_NOMBRE-1);

        // Comando leave
        } else if (strcmp(linea, "leave") == 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala.\n");
                continue;
            }
            memset(&m,0,sizeof(m));
            m.mtype = T_LEAVE;
            strncpy(m.remitente, nombre_usuario, MAX_NOMBRE-1);
            strncpy(m.sala, sala_actual, MAX_NOMBRE-1);
            m.qid_cliente = cola_privada;
            msgsnd(cola_global, &m, sizeof(struct mensaje) - sizeof(long), 0);
            sala_actual[0] = '\0';

        // Mensajes normales
        } else if (strlen(linea) > 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>' primero.\n");
                continue;
            }
            memset(&m,0,sizeof(m));
            m.mtype = T_MSG;
            strncpy(m.remitente, nombre_usuario, MAX_NOMBRE-1);
            strncpy(m.sala, sala_actual, MAX_NOMBRE-1);
            strncpy(m.texto, linea, MAX_TEXTO-1);
            m.qid_cliente = cola_privada;
            msgsnd(cola_global, &m, sizeof(struct mensaje) - sizeof(long), 0);
        }
    }

    return 0;
}
