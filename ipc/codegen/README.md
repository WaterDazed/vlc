# IPC code generator

**VLC** uses a great number of structures internally to have process separation we need a way to serialize, send and receive them easily.

For this a bison/flex parser is introduces to generate structures, send and receive functions from a simple description format.

# Description File

An IPC descriptor file describes one or a serie of messages and their content using the following format:

```
message foo {
  int64 i64;
  uint32 u32[4];
  buffer buff;
  string str[];
}
```

When run through the parser it will generate a header and a C file containing a newly created structure and 2 send/receive APIs and a cleanup helper.

Generated header:

```
struct vlc_ipc_foo {
    int64_t i64;
    uint32_t u32[4];
    size_t buff_len;
    void *buff;
    size_t str_len;
    char **str;
};

#define VLC_IPC_FOO_INIT {.i64 = 0, .u32 = {0}, .buff_len = 0, .buff = NULL, .str_len = 0, .str = NULL}

int vlc_ipc_send_foo(int fd, const struct vlc_ipc_foo *foo);
int vlc_ipc_recv_foo(int fd, struct vlc_ipc_foo *foo);
void vlc_ipc_cleanup_foo(struct vlc_ipc_foo *foo);

static inline int vlc_ipc_cmd_send_foo(struct vlc_ipc_client *client, const void *data) {
    return vlc_ipc_send_foo(client->send_fd, (const struct vlc_ipc_foo *) data);
}

static inline int vlc_ipc_cmd_recv_foo(struct vlc_ipc_client *client, void *data) {
    return vlc_ipc_recv_foo(client->recv_fd, (struct vlc_ipc_foo *) data);
}
```

Generated code:

```
int vlc_ipc_send_foo(int fd, const struct vlc_ipc_foo *foo) {
    int ret;
    struct iovec iovec0[] = {
        {.iov_base = (void*) &foo->i64, .iov_len=sizeof(foo->i64) },
        {.iov_base = (void*) foo->u32, .iov_len=sizeof(foo->u32) },
        {.iov_base = (void*) &foo->buff_len, .iov_len=sizeof(foo->buff_len) },
        {.iov_base = (void*) foo->buff, .iov_len=foo->buff_len },
        {.iov_base = (void*) &foo->str_len, .iov_len=sizeof(foo->str_len) }

    };

    ret = vlc_ipc_send_data(fd, iovec0, sizeof(iovec0)/sizeof(*iovec0));
    if (ret != VLC_SUCCESS)
        return ret;

    {
        size_t i;
        for (i = 0; i < foo->str_len; i++) {
            ret = vlc_ipc_send_string(fd, foo->str[i]);
            if (ret != VLC_SUCCESS)
                return ret;
        }
    }
    return VLC_SUCCESS;
}

int vlc_ipc_recv_foo(int fd, struct vlc_ipc_foo *foo) {
    int ret;
    struct iovec iovec0[] = {
        {.iov_base = (void*) &foo->i64, .iov_len=sizeof(foo->i64) },
        {.iov_base = (void*) foo->u32, .iov_len=sizeof(foo->u32) },
        {.iov_base = (void*) &foo->buff_len, .iov_len=sizeof(foo->buff_len) }

    };
    ret = vlc_ipc_recv_data(fd, iovec0, sizeof(iovec0)/sizeof(*iovec0));
    if (ret != VLC_SUCCESS)
        return ret;

    if (foo->buff_len != 0) {
        foo->buff = malloc(foo->buff_len);
        if (foo->buff == NULL)
            return VLC_ENOMEM;
    struct iovec iovec1[] = {
        {.iov_base = (void*) foo->buff, .iov_len=foo->buff_len }
    };
    ret = vlc_ipc_recv_data(fd, iovec1, sizeof(iovec1)/sizeof(*iovec1));
    if (ret != VLC_SUCCESS)
        return ret;

    } else {
         foo->buff = NULL;
    }
    struct iovec iovec2[] = {
        {.iov_base = (void*) &foo->str_len, .iov_len=sizeof(foo->str_len) }
    };
    ret = vlc_ipc_recv_data(fd, iovec2, sizeof(iovec2)/sizeof(*iovec2));
    if (ret != VLC_SUCCESS)
        return ret;

    if (foo->str_len != 0) {
        size_t i;
        foo->str = malloc(foo->str_len * sizeof(char *));
        if (foo->str == NULL)
            return VLC_ENOMEM;
        for (i = 0; i < foo->str_len; i++) {
            ret = vlc_ipc_recv_string(fd, &foo->str[i]);
            if (ret != VLC_SUCCESS) {
                size_t j;
                for (j = 0; j < i; j++) {
                    free(foo->str[j]);
                }
                free(foo->str);
                foo->str = NULL;
                return ret; 
            }
        }
    } else {
        foo->str = NULL;
    }
    return VLC_SUCCESS;
}

void vlc_ipc_cleanup_foo(struct vlc_ipc_foo *foo) {
    free(foo->buff);
    foo->buff = NULL;
    foo->buff_len = 0;
    for(size_t i = 0; i < foo->str_len; i++)
        free(foo->str[i]);
    free(foo->str);
    foo->str_len = 0;
    foo->str = NULL;
}

```

# Syntax
## message
The `message` keyword and its `name` identifier defines an IPC unit and will generate a `struct vlc_ipc_<name>` that can be freely exchanged through the `vlc_ipc_recv_<name>()` and `vlc_ipc_send_<name>()` automatically.

After `message` a name is required in order to generate a name.

## members
The members of a message follow the syntax:

`type identifier;`

The accepted types in `message` are the following:
* `int64`
* `uint64`
* `int32`
* `uint32`
* `int8`
* `uint8`
* `size`
* `double`
* `float`
* `string` for null terminated strings and will be translated as a char * in the generated structure
* `buffer` that will define two parameters one `size_t <name>_len;` and `void *<name>;` in the generated structure
* `message <msg_name>` a previously declared `message` that will generate the complete `struct vlc_ipc_<name> <identifier>` in the generated structure, not a pointer to it.

##array modifer
All members (except `buffer` for practical code generation reasons) accept an array modifier:

```
type identifier[];
type identifier[count];
```

* `type identifier[];` will produce 2 fields in the structure:

```
ctype identifer_len;
 ctype *identifier;
```

* `type identifier[count];` will produce only one field in the structure:

```
ctype identifer_len[count];
```

If you need an array of buffer consider wrapping them in a message.

# Warning

* Enforcing the uniqueness of message names and parameters in order to prevent collisions is let to the writers and users of the IPC descriptors.
* The cleanup helper function is meant to be called on a received structure since it only frees the buffer allocated by the recv functions.

