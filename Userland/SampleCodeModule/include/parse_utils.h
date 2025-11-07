#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

static inline int parse_int_token(const char *token, int *value) {
	if (token == NULL || token[0] == '\0' || value == NULL) {
		return 0;
	}

	int sign = 1;
	int index = 0;
	if (token[0] == '-') {
		sign = -1;
		index++;
		if (token[index] == '\0') {
			return 0;
		}
	}

	int parsed = 0;
	while (token[index] != '\0') {
		char c = token[index++];
		if (c < '0' || c > '9') {
			return 0;
		}
		parsed = parsed * 10 + (c - '0');
	}

	*value = sign * parsed;
	return 1;
}

#endif
