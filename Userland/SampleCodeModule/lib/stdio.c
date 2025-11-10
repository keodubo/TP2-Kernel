#include <stdio.h>
#include <sys_calls.h>
#include <stdarg.h>
#include <stdint.h>


// Funcion auxiliar para convertir entero a string
static void itoa(int64_t value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int64_t tmp_value;
    int is_negative = 0;

    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    if (is_negative) {
        *ptr++ = '-';
    }

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Funcion auxiliar para convertir entero sin signo a string
static void utoa(uint64_t value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    uint64_t tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Implementacion simple de printf
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    char *ptr = buffer;
    int count = 0;
    
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 'd' || *format == 'i') {
                int val = va_arg(args, int);
                char num_str[32];
                itoa(val, num_str, 10);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 'l' && *(format + 1) == 'd') {
                // Handle %ld format
                format++; // Skip the 'l'
                int64_t val = va_arg(args, int64_t);
                char num_str[32];
                itoa(val, num_str, 10);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char num_str[32];
                utoa(val, num_str, 10);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                char num_str[32];
                utoa(val, num_str, 16);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 's') {
                char *s = va_arg(args, char*);
                if (s) {
                    while (*s) {
                        *ptr++ = *s++;
                        count++;
                    }
                }
            } else if (*format == 'c') {
                char c = (char)va_arg(args, int);
                *ptr++ = c;
                count++;
            } else if (*format == '%') {
                *ptr++ = '%';
                count++;
            }
            format++;
        } else {
            *ptr++ = *format++;
            count++;
        }
        
        // Vaciar buffer si se esta llenando
        if (ptr - buffer > 1000) {
            sys_write_fd(1, buffer, ptr - buffer);
            ptr = buffer;
        }
    }
    
    // Vaciar el buffer restante
    if (ptr > buffer) {
        sys_write_fd(1, buffer, ptr - buffer);
    }
    
    va_end(args);
    return count;
}

int sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char *ptr = str;
    int count = 0;
    
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 'd' || *format == 'i') {
                int val = va_arg(args, int);
                char num_str[32];
                itoa(val, num_str, 10);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 'l' && *(format + 1) == 'd') {
                // Handle %ld format
                format++; // Skip the 'l'
                int64_t val = va_arg(args, int64_t);
                char num_str[32];
                itoa(val, num_str, 10);
                char *s = num_str;
                while (*s) {
                    *ptr++ = *s++;
                    count++;
                }
            } else if (*format == 's') {
                char *s = va_arg(args, char*);
                if (s) {
                    while (*s) {
                        *ptr++ = *s++;
                        count++;
                    }
                }
            } else if (*format == 'c') {
                char c = (char)va_arg(args, int);
                *ptr++ = c;
                count++;
            }
            format++;
        } else {
            *ptr++ = *format++;
            count++;
        }
    }
    
    *ptr = '\0';
    va_end(args);
    return count;
}

// Implementación de puts - escribe string + newline a stdout
int puts(const char *str) {
    if (str == NULL) {
        return -1;
    }
    
    int len = 0;
    const char *s = str;
    while (*s++) {
        len++;
    }
    
    sys_write_fd(1, str, len);
    sys_write_fd(1, "\n", 1);
    return len + 1;
}

// Implementación de putchar - escribe un carácter a stdout
int putchar(int c) {
    char ch = (char)c;
    sys_write_fd(1, &ch, 1);
    return (unsigned char)c;
}
