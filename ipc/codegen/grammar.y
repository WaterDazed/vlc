/*****************************************************************************
 * grammar.y
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 ****************************************************************************/

%{
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

extern FILE *yyin;
extern void yyerror(const char *s);
extern int yylex(void);

struct field {
	int type;
	char *type_str;
	char *var;
	int64_t array_sz; /* -1 is unknown array size, 0 means that it's not an array */
};

struct msg {
	char *name;
	size_t field_num;
	struct field *fields;
};

enum iovec_state {
  INIT,
  IN_IOVEC
};

static size_t msg_num = 0;
static struct msg *messages = NULL;
static struct msg current = {NULL, 0, NULL};

static struct msg *find_msg_by_name(const char *name) {
	size_t i;
	for (i = 0; i < msg_num; i++) {
		if (strcmp(name, messages[i].name) == 0)
			return &messages[i];
	}
	return NULL;
}

static void msg_append(char *name) {
	size_t i;
	if (find_msg_by_name(name) != NULL) {
		abort();
	}
	size_t new_size = msg_num + 1;
	struct msg *tmp = realloc(messages, new_size * sizeof(struct msg));
	if (tmp == NULL) {
		abort();
	}
	messages = tmp;
	messages[msg_num].name = name;
	messages[msg_num].field_num = current.field_num;
	messages[msg_num].fields = current.fields;
	current.field_num = 0;
	current.fields = 0;
	msg_num = new_size;
}

static void field_append(int type, char *type_str, char *var, int array_sz) {
	size_t new_size = current.field_num + 1;
	struct field *tmp = realloc(current.fields, new_size * sizeof(struct field));
	if (tmp == NULL) {
		abort();
	}

	current.fields = tmp;
	tmp = &current.fields[current.field_num++];
	tmp->type = type;
	tmp->type_str = type_str;
	tmp->var = var;
	tmp->array_sz = array_sz;
}

%}

%union {
	char *string;
	int64_t integer;
}

%token <string> TYPE_GENERIC
%token <string> TYPE_GENERIC_T
%token <string> IDENTIFIER
%token <integer> INTEGER
%token TYPE_STRING
%token TYPE_BUFFER
%token MESSAGE

%%

parse:
	messages	;

messages:
	message
|	messages message ;

message:
MESSAGE IDENTIFIER '{' fields '}'	{ msg_append($2); }

fields:
	field
|	fields field ;

field:
	TYPE_GENERIC IDENTIFIER ';'	{ field_append(TYPE_GENERIC, $1, $2, 0); }
|	TYPE_GENERIC IDENTIFIER '[' ']' ';'	{ field_append(TYPE_GENERIC, $1, $2, -1); }
|	TYPE_GENERIC IDENTIFIER '[' INTEGER ']' ';'	{ field_append(TYPE_GENERIC, $1, $2, $4); }

|	TYPE_GENERIC_T IDENTIFIER ';'	{ field_append(TYPE_GENERIC_T, $1, $2, 0); }
|	TYPE_GENERIC_T IDENTIFIER '[' ']' ';'	{ field_append(TYPE_GENERIC_T, $1, $2, -1); }
|	TYPE_GENERIC_T IDENTIFIER '[' INTEGER ']' ';'	{ field_append(TYPE_GENERIC_T, $1, $2, $4); }

|	TYPE_STRING IDENTIFIER ';'	{ field_append(TYPE_STRING, NULL, $2, 0); }
|	TYPE_STRING IDENTIFIER '[' ']' ';'	{ field_append(TYPE_STRING, NULL, $2, -1); }
|	TYPE_STRING IDENTIFIER '[' INTEGER ']' ';'	{ field_append(TYPE_STRING, NULL, $2, $4); }

|	MESSAGE IDENTIFIER IDENTIFIER ';'	{ field_append(MESSAGE, $2, $3, 0); }
|	MESSAGE IDENTIFIER IDENTIFIER '[' ']' ';'	{ field_append(MESSAGE, $2, $3, -1); }
|	MESSAGE IDENTIFIER IDENTIFIER '[' INTEGER ']' ';'	{ field_append(MESSAGE, $2, $3, $5); }

|	TYPE_BUFFER IDENTIFIER ';'	{ field_append(TYPE_BUFFER, NULL, $2, 0); }

%%

void yyerror(const char *s) {
	fprintf(stderr, "Error: %s\n", s);
}

char *toinit(const char *name) {
	const char *fmt = "VLC_IPC_%s_INIT";
	char *str = malloc(strlen(fmt)-2+strlen(name));
	if (str == NULL)
		abort();
	sprintf(str, fmt, name);
	size_t i;
	for (i = 7; str[i] != '\0'; i++) {
		str[i] = toupper(str[i]);
	}
	return str;
}

/*****************************************************************************
 * Header generation
 ****************************************************************************/
static void write_init_field(const struct field *f, FILE *out) {
	switch (f->type) {
	case TYPE_GENERIC:
	case TYPE_GENERIC_T:
		if(f->array_sz < 0) {
			fprintf(out, ".%s_len = 0, .%s = NULL", f->var,  f->var);
		} else if (f->array_sz > 0) {
			fprintf(out, ".%s = {0}", f->var);
		} else {
			fprintf(out, ".%s = 0", f->var);
		}
		break;
	case TYPE_STRING:
		if(f->array_sz < 0) {
			fprintf(out, ".%s_len = 0, .%s = NULL", f->var,  f->var);
		} else if (f->array_sz > 0) {
			fprintf(out, ".%s = {NULL}", f->var);
		} else {
			fprintf(out, ".%s = NULL", f->var);
		}
		break;
	case MESSAGE:
		if(f->array_sz < 0) {
			fprintf(out, ".%s_len = 0, .%s = NULL", f->var,  f->var);
		} else if (f->array_sz > 0) {
			char *init = toinit(f->type_str);
			int64_t i;
			fprintf(out, ".%s = {", f->var);
			for (i = 0; i < f->array_sz; i++) {
				if (i != 0)
					fprintf(out, " ,");
				fprintf(out, "%s", init);
			}
			free(init);
			fprintf(out, "}");
		} else {
			char *init = toinit(f->type_str);
			fprintf(out, ".%s = %s",
				f->var, init);
			free(init);
		}
	break;
		case TYPE_BUFFER:
			fprintf(out, ".%s_len = 0, .%s = NULL", f->var,  f->var);
		break;
	}
}

static void write_field(const struct field *f, FILE *out) {
	switch (f->type) {
	case TYPE_GENERIC:
		if(f->array_sz < 0) {
			fprintf(out,
				"    size_t %s_len;\n"
				"    %s *%s;\n",
				f->var, f->type_str, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    %s %s[%"PRId64"];\n",
				f->type_str, f->var, f->array_sz);
		} else {
			fprintf(out,
				"    %s %s;\n",
				f->type_str, f->var);
		}
		break;
	case TYPE_GENERIC_T:
		if(f->array_sz < 0) {
			fprintf(out,
				"    size_t %s_len;\n"
				"    %s_t *%s;\n",
				f->var, f->type_str, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    %s_t %s[%"PRId64"];\n",
				f->type_str, f->var, f->array_sz);
		} else {
			fprintf(out,
				"    %s_t %s;\n",
				f->type_str, f->var);
		}
		break;
	case TYPE_STRING:
		if(f->array_sz < 0) {
			fprintf(out,
				"    size_t %s_len;\n"
				"    char **%s;\n",
				f->var, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    char *%s[%"PRId64"];\n",
				f->var, f->array_sz);
		} else {
			fprintf(out,
				"    char *%s;\n",
				f->var);
		}
		break;
	case MESSAGE:
		if(f->array_sz < 0) {
			fprintf(out,
				"    size_t %s_len;\n"
				"    struct vlc_ipc_%s *%s;\n",
				f->var, f->type_str, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    struct vlc_ipc_%s %s[%"PRId64"];\n",
				f->type_str, f->var, f->array_sz);
		} else {
			fprintf(out,
				"    struct vlc_ipc_%s %s;\n",
				f->type_str, f->var);
		}
	break;
		case TYPE_BUFFER:
			fprintf(out,
				"    size_t %s_len;\n"
				"    void *%s;\n",
				f->var, f->var);
		break;
	}
}

static void write_header(const struct msg *m, FILE *out) {
	const char *name = m->name;
	fprintf(out, "struct vlc_ipc_%s {\n", m->name);
	size_t i;
	for (i = 0; i < m->field_num; i++) {
		write_field(&m->fields[i], out);
	}
	char *init_str = toinit(m->name);
	fprintf(out,
		"};\n\n"
		"#define %s {",
		init_str);
	free(init_str);
	for (i = 0; i < m->field_num; i++) {
		if (i != 0) fprintf(out, ", ");
		write_init_field(&m->fields[i], out);
	}
	fprintf(out,
		"}\n\n"
		"int vlc_ipc_send_%s(int fd, const struct vlc_ipc_%s *%s);\n"
		"int vlc_ipc_recv_%s(int fd, struct vlc_ipc_%s *%s);\n"
		"void vlc_ipc_cleanup_%s(struct vlc_ipc_%s *%s);\n\n"
		"static inline int vlc_ipc_cmd_send_%s(struct vlc_ipc_client *client, const void *data) {\n"
		"    return vlc_ipc_send_%s(client->send_fd, (const struct vlc_ipc_%s *) data);\n"
		"}\n\n"
		"static inline int vlc_ipc_cmd_recv_%s(struct vlc_ipc_client *client, void *data) {\n"
		"    return vlc_ipc_recv_%s(client->recv_fd, (struct vlc_ipc_%s *) data);\n"
		"}\n\n",
		name, name, name, name, name, name, name, name, name,
		name, name, name, name, name, name);
}


/*****************************************************************************
 * Code generation
 ****************************************************************************/
static void open_append_iovec(FILE *out, size_t *iovec_idx, enum iovec_state *state) {
	if (*state == INIT) {
		fprintf(out, "    struct iovec iovec%zu[] = {\n", *iovec_idx);
		*state = IN_IOVEC;
	} else {
		fprintf(out, ",\n");
	}
}

static void close_send_iovec(FILE *out, size_t *iovec_idx, enum iovec_state *state) {
	size_t idx = *iovec_idx;
	if (*state == IN_IOVEC) {
		fprintf(out,
			"\n    };\n\n"
			"    ret = vlc_ipc_send_data(fd, iovec%zu, sizeof(iovec%zu)/sizeof(*iovec%zu));\n"
			"    if (ret != VLC_SUCCESS)\n"
			"        return ret;\n\n",
			idx, idx, idx);
		*iovec_idx = idx+1;
		*state = INIT;
	}
}

static void send_string_array(const char *struct_name, const char *var_name, const char *string_num, size_t *iovec_idx, enum iovec_state *state, FILE *out) {
	close_send_iovec(out, iovec_idx, state);
	fprintf(out,
		"    {\n"
		"        size_t i;\n"
		"        for (i = 0; i < %s; i++) {\n"
		"            ret = vlc_ipc_send_string(fd, %s->%s[i]);\n"
		"            if (ret != VLC_SUCCESS)\n"
		"                return ret;\n"
		"    }\n",
		string_num, struct_name, var_name);
}

static void write_iovec_send_field(struct field *f, const char *name, size_t *iovec_idx, enum iovec_state *state, FILE *out) {
	switch(f->type) {
	case TYPE_GENERIC:
	case TYPE_GENERIC_T:
		open_append_iovec(out, iovec_idx, state);
		if (f->array_sz < 0) {
			fprintf(out,
							"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) },\n"
							"        {.iov_base = (void*) %s->%s, .iov_len=(%s->%s_len * sizeof(%s%s)) }",
								name, f->var, name, f->var, name, f->var, name, f->var, f->type_str, (f->type == TYPE_GENERIC_T?"_t":""));
		} else if (f->array_sz > 0) {
			fprintf(out, "        {.iov_base = (void*) %s->%s, .iov_len=sizeof(%s->%s) }",
								name, f->var, name, f->var);
		} else {
			fprintf(out, "        {.iov_base = (void*) &%s->%s, .iov_len=sizeof(%s->%s) }",
								name, f->var, name, f->var);
		}
		break;
	case TYPE_BUFFER:
		open_append_iovec(out, iovec_idx, state);
		fprintf(out,
						"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) },\n"
						"        {.iov_base = (void*) %s->%s, .iov_len=%s->%s_len }",
						name, f->var, name, f->var, name, f->var, name, f->var);
		break;
	case TYPE_STRING:

		if (f->array_sz < 0) {
			open_append_iovec(out, iovec_idx, state);
			fprintf(out,
				"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }\n",
				name, f->var, name, f->var);
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %s->%s_len; i++) {\n"
				"            ret = vlc_ipc_send_string(fd, %s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS)\n"
				"                return ret;\n"
				"        }\n"
				"    }\n",
				name, f->var, name, f->var);
		} else if (f->array_sz > 0) {
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %"PRId64"; i++) {\n"
				"            ret = vlc_ipc_send_string(fd, %s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS)\n"
				"                return ret;\n"
				"        }\n"
				"    }\n",
				f->array_sz, name, f->var);
		} else {
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    ret = vlc_ipc_send_string(fd, %s->%s);\n"
				"    if (ret != VLC_SUCCESS)\n"
				"        return ret;\n\n",
				name, f->var);
		}

		break;
	case MESSAGE:
		if (f->array_sz < 0) {
			open_append_iovec(out, iovec_idx, state);
			fprintf(out,
				"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }\n",
				name, f->var, name, f->var);
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %s->%s_len; i++) {\n"
				"            ret = vlc_ipc_send_%s(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS)\n"
				"                return ret;\n"
				"        }\n"
				"    }\n",
				name, f->var, f->type_str, name, f->var);
		} else if (f->array_sz > 0) {
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %"PRId64"; i++) {\n"
				"            ret = vlc_ipc_send_%s(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS)\n"
				"                return ret;\n"
				"        }\n"
				"    }\n",
				f->array_sz, f->type_str, name, f->var);
		} else {
			close_send_iovec(out, iovec_idx, state);
			fprintf(out,
				"    ret = vlc_ipc_send_%s(fd, &%s->%s);\n"
				"    if (ret != VLC_SUCCESS)\n"
				"        return ret;\n\n",
				f->type_str, name, f->var);
		}
	}
}

static void write_send(struct msg *m, FILE *out) {
	const char *name = m->name;
	size_t iovec_idx = 0;
	enum iovec_state state = INIT;
	fprintf(out,
		"int vlc_ipc_send_%s(int fd, const struct vlc_ipc_%s *%s) {\n"
		"    int ret;\n",
		name, name, name);
	size_t i;
	for (i = 0; i < m->field_num; i++) {
		write_iovec_send_field(&m->fields[i], name, &iovec_idx, &state, out);
	}
	close_send_iovec(out, &iovec_idx, &state);
	fprintf(out,
		"    return VLC_SUCCESS;\n"
		"}\n\n");
}

static void close_recv_iovec(FILE *out, size_t *iovec_idx, enum iovec_state *state) {
	size_t idx = *iovec_idx;
	if (*state == IN_IOVEC) {
		fprintf(out,
			"\n    };\n"
			"    ret = vlc_ipc_recv_data(fd, iovec%zu, sizeof(iovec%zu)/sizeof(*iovec%zu));\n"
			"    if (ret != VLC_SUCCESS)\n"
			"        return ret;\n\n",
			idx, idx, idx);
		*iovec_idx = idx+1;
		*state = INIT;
	}
}

static void write_iovec_recv_field(struct field *f, const char *name, size_t *iovec_idx, enum iovec_state *state, FILE *out) {
	switch(f->type) {
	case TYPE_GENERIC:
	case TYPE_GENERIC_T:
		open_append_iovec(out, iovec_idx, state);
		if (f->array_sz < 0) {
			fprintf(out,
				"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }",
				name, f->var, name, f->var);
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    if (%s->%s_len != 0) {\n"
				"        %s->%s = malloc(%s->%s_len * sizeof(%s%s));\n"
				"        if (%s->%s == NULL)\n"
				"            return VLC_ENOMEM;\n    ",
				name, f->var, name, f->var, name, f->var, f->type_str, (f->type == TYPE_GENERIC_T?"_t":""), name, f->var);
			open_append_iovec(out, iovec_idx, state);
			fprintf(out,
				"            {.iov_base = (void*) %s->%s, .iov_len=(%s->%s_len*sizeof(%s%s))}",
				name, f->var, name, f->var, f->type_str, (f->type == TYPE_GENERIC_T?"_t":""));
		close_recv_iovec(out, iovec_idx, state);
		fprintf(out,
			"    } else {\n"
			"         %s->%s = NULL;\n"
		  "    }\n",
			name, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out, "        {.iov_base = (void*) %s->%s, .iov_len=sizeof(%s->%s) }",
				name, f->var, name, f->var);
		} else {
			fprintf(out, "        {.iov_base = (void*) &%s->%s, .iov_len=sizeof(%s->%s) }",
				name, f->var, name, f->var);
		}
		break;
	case TYPE_BUFFER:
		open_append_iovec(out, iovec_idx, state);
		fprintf(out,
			"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }\n",
			name, f->var, name, f->var);
		close_recv_iovec(out, iovec_idx, state);
		fprintf(out,
			"    if (%s->%s_len != 0) {\n"
			"        %s->%s = malloc(%s->%s_len);\n"
			"        if (%s->%s == NULL)\n"
			"            return VLC_ENOMEM;\n",
			name, f->var, name, f->var, name, f->var, name, f->var);
		open_append_iovec(out, iovec_idx, state);
		fprintf(out,
			"        {.iov_base = (void*) %s->%s, .iov_len=%s->%s_len }",
			name, f->var, name, f->var);
		close_recv_iovec(out, iovec_idx, state);
		fprintf(out,
			"    } else {\n"
			"         %s->%s = NULL;\n"
		  "    }\n",
			name, f->var);
		break;
	case TYPE_STRING:

		if (f->array_sz < 0) {
			open_append_iovec(out, iovec_idx, state);
			fprintf(out,
				"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }",
				name, f->var, name, f->var);
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    if (%s->%s_len != 0) {\n"
				"        size_t i;\n"
				"        %s->%s = malloc(%s->%s_len * sizeof(char *));\n"
				"        if (%s->%s == NULL)\n"
				"            return VLC_ENOMEM;\n"
				"        for (i = 0; i < %s->%s_len; i++) {\n"
				"            ret = vlc_ipc_recv_string(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS) {\n"
				"                size_t j;\n"
				"                for (j = 0; j < i; j++) {\n"
				"                    free(%s->%s[j]);\n"
				"                }\n"
				"                free(%s->%s);\n"
				"                %s->%s = NULL;\n"
				"                return ret; \n"
				"            }\n"
				"        }\n"
				"    } else {\n"
				"        %s->%s = NULL;\n"
				"    }\n",
				name, f->var, name, f->var, name, f->var, name, f->var, name, f->var, name, f->var, name, f->var, name, f->var, name, f->var, name, f->var);
		} else if (f->array_sz > 0) {
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %"PRId64"; i++) {\n"
				"            ret = vlc_ipc_recv_string(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS) {\n"
				"                size_t j;\n"
				"                for (j = 0; j < i; j++) {\n"
				"                    free(%s->%s[j]);\n"
				"                    %s->%s[j] = NULL;\n"
				"                }\n"
				"                return ret; \n"
				"            }\n"
				"        }\n"
				"    }\n",
				f->array_sz, name, f->var, name, f->var, name, f->var);
		} else {
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    ret = vlc_ipc_recv_string(fd, &%s->%s);\n"
				"    if (ret != VLC_SUCCESS)\n"
				"        return ret;\n\n",
				name, f->var);
		}

		break;
	case MESSAGE:
		if (f->array_sz < 0) {
			open_append_iovec(out, iovec_idx, state);
			fprintf(out,
				"        {.iov_base = (void*) &%s->%s_len, .iov_len=sizeof(%s->%s_len) }",
				name, f->var, name, f->var);
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    if (%s->%s_len) {\n"
				"        size_t i;\n"
				"        %s->%s = malloc(%s->%s_len * sizeof(struct vlc_ipc_%s));\n"
				"        if (%s->%s == NULL)\n"
				"            return VLC_ENOMEM;\n"
				"        for (i = 0; i < %s->%s_len; i++) {\n"
				"            ret = vlc_ipc_recv_%s(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS) {\n"
				"                size_t j;\n"
				"                for (j = 0; j < i; j++) {\n"
				"                    vlc_ipc_cleanup_%s(&%s->%s[j]);\n"
				"                }\n"
				"                free(%s->%s);\n"
				"                %s->%s = NULL;\n"
				"                return ret; \n"
				"            }\n"
				"        }\n"
				"    } else {\n"
				"        %s->%s = NULL;\n"
				"    }\n",
				name, f->var, name, f->var, name, f->var, f->type_str, name, f->var, name, f->var, f->type_str, name, f->var, f->type_str, name, f->var, name, f->var, name, f->var, name, f->var);
		} else if (f->array_sz > 0) {
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    {\n"
				"        size_t i;\n"
				"        for (i = 0; i < %"PRId64"; i++) {\n"
				"            ret = vlc_ipc_recv_%s(fd, &%s->%s[i]);\n"
				"            if (ret != VLC_SUCCESS) {\n"
				"                size_t j;\n"
				"                for (j = 0; j < i; j++) {\n"
				"                    vlc_ipc_cleanup_%s(&%s->%s[j]);\n"
				"                }\n"
				"                return ret; \n"
				"            }\n"
				"        }\n"
				"    }\n",
				f->array_sz, f->type_str, name, f->var, f->type_str, name, f->var);
		} else {
			close_recv_iovec(out, iovec_idx, state);
			fprintf(out,
				"    ret = vlc_ipc_recv_%s(fd, &%s->%s);\n"
				"    if (ret != VLC_SUCCESS)\n"
				"        return ret;\n\n",
				f->type_str, name, f->var);
		}

		break;
	}
}

static void write_recv(struct msg *m, FILE *out) {
	const char *name = m->name;
	size_t iovec_idx = 0;
	enum iovec_state state = INIT;

	fprintf(out,
		"int vlc_ipc_recv_%s(int fd, struct vlc_ipc_%s *%s) {\n"
		"    int ret;\n",
		name, name, name);
	size_t i;
	for (i = 0; i < m->field_num; i++) {
		write_iovec_recv_field(&m->fields[i], name, &iovec_idx, &state, out);
	}
	close_recv_iovec(out, &iovec_idx, &state);
	fprintf(out, "    return VLC_SUCCESS;\n}\n\n");
}

static void write_cleanup_field(const struct field *f, const char *name, FILE *out) {
		switch (f->type) {
	case TYPE_GENERIC:
	case TYPE_GENERIC_T:
		if (f->array_sz < 0)
			fprintf(out,
				"    free(%s->%s);\n"
				"    %s->%s = NULL;\n"
				"    %s->%s_len = 0;\n",
				name, f->var, name, f->var, name, f->var);
		break;
	case TYPE_STRING:
		if (f->array_sz < 0) {
			fprintf(out,
				"    for(size_t i = 0; i < %s->%s_len; i++)\n"
				"        free(%s->%s[i]);\n"
				"    free(%s->%s);\n"
				"    %s->%s_len = 0;\n"
				"    %s->%s = NULL;\n",
				name, f->var, name, f->var, name, f->var, name, f->var, name, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    for(size_t i = 0; i < %"PRId64"; i++)\n {"
				"        free(%s->%s[i]);\n"
				"        %s->%s[i] = NULL;\n"
				"    }\n",
				f->array_sz, name, f->var, name, f->var);
		} else {
			fprintf(out,
				"    free(%s->%s);\n"
				"    %s->%s = NULL;\n",
				name, f->var, name, f->var);
		}
		break;
	case TYPE_BUFFER:
		fprintf(out,
			"    free(%s->%s);\n"
			"    %s->%s = NULL;\n"
			"    %s->%s_len = 0;\n",
			name, f->var, name, f->var, name, f->var);
		break;
	case MESSAGE:
		if (f->array_sz < 0) {
			fprintf(out,
				"    for(size_t i = 0; i < %s->%s_len; i++)\n"
				"         vlc_ipc_cleanup_%s(&%s->%s[i]);\n"
				"    free(%s->%s);\n"
				"    %s->%s_len = 0;\n"
				"    %s->%s = NULL;\n",
				name, f->var, f->type_str, name, f->var, name, f->var, name, f->var, name, f->var);
		} else if (f->array_sz > 0) {
			fprintf(out,
				"    for(size_t i = 0; i < %"PRId64"; i++) {\n"
				"        vlc_ipc_cleanup_%s(&%s->%s[i]);\n"
				"    }\n",
				f->array_sz, f->type_str, name, f->var);
		} else {
			fprintf(out,
				"    vlc_ipc_cleanup_%s(&%s->%s);\n",
				f->type_str, name, f->var);
		}
		break;
	default:
		break;
	}
}

static void write_cleanup(struct msg *m, FILE *out) {
	const char *name = m->name;
	fprintf(out, "void vlc_ipc_cleanup_%s(struct vlc_ipc_%s *%s) {\n",
		name, name, name);
	size_t i;
	for (i = 0; i < m->field_num; i++) {
		write_cleanup_field(&m->fields[i], name, out);
	}
	fprintf(out, "}\n\n");
}

int main(int argc, char *argv[]) {
	size_t i, j;

	if (argc != 4) {
		fprintf(stderr, "Usage %s <input.h> <output.c> <output.h>\n", argv[0]);
		return -1;
	}

	char *ifd = strdup(argv[3]);
	if (ifd == NULL)
		return -1;

	for (char *c = ifd; *c != '\0'; c++) {
		if (isalnum(*c))
			*c = toupper(*c);
		else
			*c = '_';
	}

	yyin = fopen(argv[1], "ro");
	FILE *outc = fopen(argv[2], "w");
	FILE *outh = fopen(argv[3], "w");

	fprintf(outh,
		"// SPDX-License-Identifier: LGPL-2.1-or-later\n"
		"// WARNING: This code has been generated it should not be modified\n"
		"#ifndef VLC_IPC_%s\n"
		"#define VLC_IPC_%s\n"
		"#include \"ipc.h\"\n"
		"#include \"client.h\"\n\n"
		"#ifdef __cplusplus\n"
		"extern \"C\" {\n"
		"#endif\n\n", ifd, ifd);

	fprintf(outc,
		"// SPDX-License-Identifier: LGPL-2.1-or-later\n"
		"// WARNING: This code has been generated it should not be modified\n"
		"#ifdef HAVE_CONFIG_H\n"
		"#include \"config.h\"\n"
		"#endif\n"
		"#include <vlc_common.h>\n"
		"#ifdef HAVE_SYS_UIO_H\n"
		"#include <sys/uio.h>\n"
		"#endif\n"
		"#include \"%s\"\n\n",
		argv[3]);

	yyparse();
	for (i = 0; i < msg_num; i++) {
		struct msg *m = &messages[i];
#ifdef DEBUG_PARSING
		fprintf(stderr, "\nmessage %s:\n", m->name);
		for (j = 0; j < m->field_num; j++) {
			struct field *f = &m->fields[j];
			fprintf(stderr, "\t%d %s %s %"PRId64"\n", f->type, f->type_str,f->var,f->array_sz);
		}
#endif
		write_header(m, outh);
		write_send(m, outc);
		write_recv(m, outc);
		write_cleanup(m, outc);
	}

		fprintf(outh,
		"\n\n#ifdef __cplusplus\n"
		"}\n"
		"#endif\n"
		"#endif /* VLC_IPC_%s */\n", ifd);

	free(ifd);
	fflush(outc);
	fflush(outh);
	fclose(yyin);
	fclose(outc);
	fclose(outh);

	for (i = 0; i < msg_num; i++) {
		struct msg *m = &messages[i];
		free(m->name);
		for (j = 0; j < m->field_num; j++) {
			struct field *f = &m->fields[j];
			free(f->type_str);
			free(f->var);
		}
	}


	return 0;
}
