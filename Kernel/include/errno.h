#ifndef ERRNO_H
#define ERRNO_H

/**
 * @file errno.h
 * @brief Códigos de error del kernel
 */

#define E_SUCCESS       0   /**< Operación exitosa */
#define E_INVAL        -1   /**< Argumento inválido */
#define E_NOMEM        -2   /**< Sin memoria disponible */
#define E_NOENT        -3   /**< Entidad no encontrada */
#define E_PERM         -4   /**< Operación no permitida */
#define E_CHILD        -5   /**< No hay hijos para esperar */
#define E_AGAIN        -6   /**< Recurso temporalmente no disponible */
#define E_BG_INPUT     -7   /**< Proceso en background no puede leer stdin */
#define E_IO           -8   /**< Error de I/O */
#define E_BUSY         -9   /**< Recurso ocupado */
#define E_DEADLK      -10   /**< Deadlock detectado */

#endif /* ERRNO_H */
