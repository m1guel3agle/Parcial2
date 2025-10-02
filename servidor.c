#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>

// ------------------- Constantes de configuración -------------------
#define MAX_SALAS 10                   // Máximo de salas disponibles
#define MAX_USUARIOS_POR_SALA 50       // Máximo de usuarios por sala
#define MAX_TEXTO 256                  // Longitud máxima de un mensaje
#define MAX_NOMBRE 50                  // Longitud máxima de nombre de usuario/sala

// Tipos de mensajes que usaremos en el sistema
#define T_JOIN 1   // Cliente solicita unirse a sala
#define T_RESP 2   // Respuesta del servidor a cliente
#define T_MSG 3    // Mensaje normal dentro de la sala
#define T_LEAVE 4  // Cliente se retira de la sala

// ------------------- Estructuras -------------------

// Mensaje enviado entre clientes y servidor
struct mensaje {
    long mtype;                     // Tipo de mensaje (JOIN, MSG, etc.)
    char remitente[MAX_NOMBRE];     // Nombre del usuario que envía
    char texto[MAX_TEXTO];          // Texto del mensaje
    char sala[MAX_NOMBRE];          // Sala a la que pertenece
    int qid_cliente;                // Cola privada del cliente para recibir mensajes
};

// Representación de una sala con usuarios dentro
struct sala {
    char nombre[MAX_NOMBRE];                          // Nombre de la sala
    int num_usuarios;                                 // Número de usuarios en la sala
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE]; // Nombres de usuarios
    int qids[MAX_USUARIOS_POR_SALA];                  // Colas privadas asociadas
};

// ------------------- Variables globales -------------------
struct sala salas[MAX_SALAS]; // Lista de salas
int num_salas = 0;            // Número actual de salas
int cola_global = -1;         // Cola global donde clientes mandan sus mensajes

// ------------------- Funciones auxiliares -------------------

// Cierra el servidor y elimina la cola global
void cleanup_and_exit(int signo) {
    (void)signo; // evitar warning por parámetro no usado
    if (cola_global != -1) {
        msgctl(cola_global, IPC_RMID, NULL); // Borrar la cola global
        printf("\nCola global eliminada. Servidor terminado.\n");
    }
    exit(0);
}

// Buscar el índice de una sala por nombre
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) return i;
    }
    return -1; // No encontrada
}

// Crear una nueva sala
int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) return -1;
    strncpy(salas[num_salas].nombre, nombre, MAX_NOMBRE-1);
    salas[num_salas].num_usuarios = 0;
    num_salas++;
    return num_salas - 1;
}

// Agregar un usuario a una sala
int agregar_usuario_a_sala(int idx_sala, const char *usuario, int qid_cliente) {
    if (idx_sala < 0 || idx_sala >= num_salas) return -1;
    struct sala *s = &salas[idx_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) return -1;

    // Evitar duplicados
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], usuario) == 0) return -1;
    }

    strncpy(s->usuarios[s->num_usuarios], usuario, MAX_NOMBRE-1);
    s->qids[s->num_usuarios] = qid_cliente;
    s->num_usuarios++;
    return 0;
}

// Quitar un usuario de la sala
int quitar_usuario_de_sala(int idx_sala, const char *usuario) {
    if (idx_sala < 0 || idx_sala >= num_salas) return -1;
    struct sala *s = &salas[idx_sala];

    int found = -1;
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], usuario) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) return -1;

    // Reemplazar con el último usuario
    s->num_usuarios--;
    if (found != s->num_usuarios) {
        strncpy(s->usuarios[found], s->usuarios[s->num_usuarios], MAX_NOMBRE);
        s->qids[found] = s->qids[s->num_usuarios];
    }
    return 0;
}

// Enviar un mensaje de respuesta a un cliente
void enviar_respuesta_a_cliente(int qid_cliente, const char *texto) {
    struct mensaje resp;
    resp.mtype = T_RESP; // Tipo respuesta
    strncpy(resp.remitente, "SERVIDOR", MAX_NOMBRE-1);
    strncpy(resp.texto, texto, MAX_TEXTO-1);

    if (msgsnd(qid_cliente, &resp, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
        perror("msgsnd respuesta cliente");
    }
}

// Reenviar un mensaje a todos los usuarios de la sala
void reenviar_msg_a_sala(int idx_sala, struct mensaje *m) {
    if (idx_sala < 0 || idx_sala >= num_salas) return;
    struct sala *s = &salas[idx_sala];

    for (int i = 0; i < s->num_usuarios; i++) {
        int q = s->qids[i];
        // No reenviar al remitente
        if (q == m->qid_cliente && strcmp(s->usuarios[i], m->remitente) == 0) continue;

        struct mensaje out;
        out.mtype = T_MSG;
        strncpy(out.remitente, m->remitente, MAX_NOMBRE-1);
        strncpy(out.texto, m->texto, MAX_TEXTO-1);
        strncpy(out.sala, m->sala, MAX_NOMBRE-1);
        out.qid_cliente = 0;

        if (msgsnd(q, &out, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("msgsnd reenviar a cliente");
        }
    }
}

// ------------------- Función principal -------------------
int main() {
    // Capturar señales para cerrar correctamente
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // Crear cola global
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;
    while (1) {
        // Recibir mensajes de clientes
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("msgrcv global");
            continue;
        }

        // Procesar según el tipo de mensaje recibido
        if (msg.mtype == T_JOIN) {
            printf("[JOIN] Usuario '%s' quiere unirse a sala '%s'\n", msg.remitente, msg.sala);
            int idx = buscar_sala(msg.sala);

            if (idx == -1) idx = crear_sala(msg.sala);

            if (agregar_usuario_a_sala(idx, msg.remitente, msg.qid_cliente) == 0) {
                char buf[300];
                snprintf(buf, sizeof(buf), "Te has unido a la sala: %s", msg.sala);
                enviar_respuesta_a_cliente(msg.qid_cliente, buf);
            } else {
                enviar_respuesta_a_cliente(msg.qid_cliente, "ERROR: No se pudo agregar a la sala.");
            }

        } else if (msg.mtype == T_MSG) {
            printf("[MSG] Sala '%s' <- %s : %s\n", msg.sala, msg.remitente, msg.texto);
            int idx = buscar_sala(msg.sala);
            if (idx != -1) reenviar_msg_a_sala(idx, &msg);

        } else if (msg.mtype == T_LEAVE) {
            printf("[LEAVE] Usuario '%s' sale de sala '%s'\n", msg.remitente, msg.sala);
            int idx = buscar_sala(msg.sala);
            if (idx != -1) {
                quitar_usuario_de_sala(idx, msg.remitente);
                enviar_respuesta_a_cliente(msg.qid_cliente, "Te has desconectado de la sala.");
            }
        }
    }

    return 0;
}
