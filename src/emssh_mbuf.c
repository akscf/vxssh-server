/**
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "emssh.h"

static const char B64T[65] ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define DEFAULT_SIZE 255

static void mem_destructor_em_ssh_mbuf_t(void *data) {
    em_ssh_mbuf_t *mb = data;

    em_ssh_mem_deref(mb->buf);
}

static inline uint32_t b64val(char c) {
	if ('A' <= c && c <= 'Z') return c - 'A' + 0;
	else if ('a' <= c && c <= 'z') return c - 'a' + 26;
	else if ('0' <= c && c <= '9') return c - '0' + 52;
	else if ('+' == c) return 62;
	else if ('/' == c) return 63;
	else if ('=' == c) return 1<<24;
	else return 0;
}
// -----------------------------------------------------------------------------------------------------------------
/**
 *
 **/
size_t em_ssh_mbuf_get_left(em_ssh_mbuf_t *mb) {
    return (mb && (mb->end > mb->pos)) ? (mb->end - mb->pos) : 0;
}

/**
 *
 **/
size_t em_ssh_mbuf_get_space(em_ssh_mbuf_t *mb) {
    return (mb && (mb->size > mb->pos)) ? (mb->size - mb->pos) : 0;
}

/**
 *
 **/
int em_ssh_mbuf_set_pos(em_ssh_mbuf_t *mb, size_t pos) {
    if (!mb) {
        return EINVAL;
    }
    mb->pos = (pos > mb->end ? mb->end : pos);
    return OK;
}

/**
 *
 **/
int em_ssh_mbuf_alloc(em_ssh_mbuf_t **mb, size_t size) {
    em_ssh_mbuf_t *tmb = NULL;

    tmb = em_ssh_mem_zalloc(sizeof(em_ssh_mbuf_t), mem_destructor_em_ssh_mbuf_t);
    if (tmb == NULL) {
        return ENOMEM;
    }

    if (em_ssh_mbuf_resize(tmb, size ? size : DEFAULT_SIZE)) {
        em_ssh_mem_deref(tmb);
        return;
    }
    *mb = tmb;

    return OK;
}

/**
 *
 **/
int em_ssh_mbuf_resize(em_ssh_mbuf_t *mb, size_t size) {
    uint8_t *buf = NULL;

    if (!mb) {
        return EINVAL;
    }
    buf = mb->buf ? em_ssh_mem_realloc(mb->buf, size) : em_ssh_mem_zalloc(size, NULL);
    if (buf == NULL) {
        return ENOMEM;
    }
    mb->buf  = buf;
    mb->size = size;
    return OK;
}

/**
 *
 **/
int em_ssh_mbuf_trim(em_ssh_mbuf_t *mb) {
    int err;

    if (!mb || !mb->end || mb->end == mb->size) {
        return;
    }

    err = em_ssh_mbuf_resize(mb, mb->end);
    return err;
}

int em_ssh_mbuf_clear(em_ssh_mbuf_t *mb) {

    if (!mb) {
        return EINVAL;
    }

    explicit_bzero(mb->buf, mb->size);
    mb->pos = 0;
    mb->end = 0;

    return OK;
}

int em_ssh_mbuf_fill(em_ssh_mbuf_t *mb, uint8_t ch, size_t size) {

    if (!mb) {
        return EINVAL;
    }

    while(size--) {
        em_ssh_mbuf_write_u8(mb, ch);
    }

    return OK;
}

/**
 *
 **/
int em_ssh_mbuf_write_mem(em_ssh_mbuf_t *mb, const uint8_t *buf, size_t size) {
    size_t rsize;

    if (!mb || !buf) {
        return EINVAL;
    }

    rsize = (mb->pos + size);
    if (rsize > mb->size) {
        const size_t dsize = (mb->size ? (mb->size * 2) : DEFAULT_SIZE);
        int err;

        err = em_ssh_mbuf_resize(mb, MAX(rsize, dsize));
        if (err != OK) return err;
    }

    memcpy(mb->buf + mb->pos, buf, size);

    mb->pos += size;
    mb->end = MAX(mb->end, mb->pos);

    return OK;
}

int em_ssh_mbuf_write_u8(em_ssh_mbuf_t *mb, uint8_t v) {
    return em_ssh_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

int em_ssh_mbuf_write_u16(em_ssh_mbuf_t *mb, uint16_t v) {
    return em_ssh_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

int em_ssh_mbuf_write_u32(em_ssh_mbuf_t *mb, uint32_t v) {
    return em_ssh_mbuf_write_mem(mb, (uint8_t *)&v, sizeof(v));
}

int em_ssh_mbuf_write_str(em_ssh_mbuf_t *mb, const char *str) {
    if (!str) {
        return EINVAL;
    }
    return em_ssh_mbuf_write_mem(mb, (const uint8_t *)str, strlen(str));
}

/**
 * zero terminated string (str\0)
 **/
int em_ssh_mbuf_write_zstr(em_ssh_mbuf_t *mb, const char *str) {
    if (!mb) {
        return EINVAL;
    }
    if(str == NULL || strlen(str) == 0) {
        return OK;
    }
    int sz = strlen(str);
    em_ssh_mbuf_write_mem(mb, (const uint8_t *) str, sz);
    em_ssh_mbuf_write_u8(mb, 0x0);
}

/**
 * length prefix string (len_i32|str)
 **/
int em_ssh_mbuf_write_str_sz(em_ssh_mbuf_t *mb, const char *str) {
    if (!mb) {
        return EINVAL;
    }

    if(str == NULL || strlen(str) == 0) {
        em_ssh_mbuf_write_u32(mb, 0);
        return OK;
    }

    int sz = strlen(str);
    em_ssh_mbuf_write_u32(mb, sz);
    em_ssh_mbuf_write_mem(mb, (const uint8_t *) str, sz);

    return OK;
}

/**
 * zero terminated
 **/
int em_ssh_mbuf_write_zmbuf(em_ssh_mbuf_t *mb, const em_ssh_mbuf_t *smb) {
    if (!mb || !smb) {
        return EINVAL;
    }

    if(smb->end == 0) {
        return OK;
    }

    em_ssh_mbuf_write_mem(mb, (const uint8_t *) smb->buf, smb->end);
    em_ssh_mbuf_write_u8(mb, 0x0);

    return OK;
}

/**
 * length prefix
 **/
int em_ssh_mbuf_write_mbuf_sz(em_ssh_mbuf_t *mb, const em_ssh_mbuf_t *smb) {
    if (!mb || !smb) {
        return EINVAL;
    }

    if(smb->end == 0) {
        em_ssh_mbuf_write_u32(mb, 0);
        return OK;
    }

    em_ssh_mbuf_write_u32(mb, smb->end);
    em_ssh_mbuf_write_mem(mb, (const uint8_t *) smb->buf, smb->end);

    return OK;
}

/**
 * length prefix
 **/
int em_ssh_mbuf_write_mem_sz(em_ssh_mbuf_t *mb, const uint8_t *buf, size_t size) {
    if (!mb || !buf || !size) {
        return EINVAL;
    }

    em_ssh_mbuf_write_u32(mb, size);
    em_ssh_mbuf_write_mem(mb, buf, size);

    return OK;
}

/**
 * decode in to mbuf
 **/
int em_ssh_mbuf_base64_decode(em_ssh_mbuf_t *mb, const char *in, size_t in_len) {
	const char *in_end = in + in_len;

	if (!in || !mb) {
		return EINVAL;
    }

	for (; in + 3 < in_end; ) {
		uint32_t v;

		v  = b64val(*in++) << 18;
		v |= b64val(*in++) << 12;
		v |= b64val(*in++) << 6;
		v |= b64val(*in++) << 0;

		em_ssh_mbuf_write_u8(mb, v>>16);

		if (!(v & (1<<30)))
            em_ssh_mbuf_write_u8(mb, (v>>8) & 0xff);

		if (!(v & (1<<24)))
            em_ssh_mbuf_write_u8(mb, (v>>0) & 0xff);
	}
    return OK;
}

/**
 * encode in to mbuf
 **/
int em_ssh_mbuf_base64_encode(em_ssh_mbuf_t *mb, const char *in, size_t in_len) {
	const char *in_end = in + in_len;

	if (!in || !mb) {
		return EINVAL;
    }

    for (; in < in_end; ) {
		int pad = 0;
		uint32_t v = 0;

		v  = *in++ << 16;
		if (in < in_end) {
			v |= *in++ << 8;
		} else {
            ++pad;
		}
		if (in < in_end) {
			v |= *in++ << 0;
		} else {
			++pad;
		}

		em_ssh_mbuf_write_u8(mb, B64T[v>>18 & 0x3f]);
		em_ssh_mbuf_write_u8(mb, B64T[v>>12 & 0x3f]);
		em_ssh_mbuf_write_u8(mb, (pad >= 2) ? '=' : B64T[v>>6  & 0x3f]);
		em_ssh_mbuf_write_u8(mb, (pad >= 1) ? '=' : B64T[v>>0  & 0x3f]);
	}
    return OK;
}

/**
 *
 **/
int em_ssh_mbuf_read_mem(em_ssh_mbuf_t *mb, uint8_t *buf, size_t *size) {
    size_t lsz = *size;

    if (!mb || !buf) {
        return EINVAL;
    }

    if(mb->pos + lsz > mb->end) {
        lsz = (mb->end - mb->pos);
    }

    memcpy(buf, mb->buf + mb->pos, lsz);

    mb->pos += lsz;
    *size = lsz;

    return OK;
}

uint8_t em_ssh_mbuf_read_u8(em_ssh_mbuf_t *mb) {
    uint8_t ret = 0;
    size_t sz = sizeof(ret);

    if(em_ssh_mbuf_read_mem(mb, &ret, &sz) == OK) {
        return ret;
    }
    return 0;
}


uint16_t em_ssh_mbuf_read_u16(em_ssh_mbuf_t *mb) {
    uint16_t ret = 0;
    size_t sz = sizeof(ret);

    if(em_ssh_mbuf_read_mem(mb, (uint8_t *) &ret, &sz) == OK) {
        return ret;
    }
    return 0;
}

uint32_t em_ssh_mbuf_read_u32(em_ssh_mbuf_t *mb) {
    uint32_t ret = 0;
    size_t sz = sizeof(ret);

    if(em_ssh_mbuf_read_mem(mb, (uint8_t *)&ret, &sz) == OK) {
        return ret;
    }
    return 0;
}

/**
 * zero terminated  string
 **/
int em_ssh_mbuf_read_str(em_ssh_mbuf_t *mb, char *str, size_t *size) {
    size_t i = 0, lsz = *size;

    if (!mb || !str) {
        return EINVAL;
    }
    if(mb->pos + lsz > mb->end) {
        lsz = (mb->end - mb->pos);
    }
    for(i = 0; i < lsz; i++) {
        const uint8_t c = em_ssh_mbuf_read_u8(mb);
        *str++ = c;
        if (c == 0x0) {
            break;
        }
    }
    *size = i;

    return OK;
}

int em_ssh_mbuf_strdup(em_ssh_mbuf_t *mb, char **strp, size_t *len) {
    size_t llen = *len;
    char *str;
    int err = OK;

    if (!mb || !strp) {
        return EINVAL;
    }

    str = em_ssh_mem_zalloc(llen + 1, NULL);
    if (str == NULL)  {
        return ENOMEM;
    }

    err = em_ssh_mbuf_read_mem(mb, (uint8_t *)str, &llen);
    if (err)  goto out;

    str[llen] = 0x0;
    *strp = str;
    *len = llen;

out:
    if (err) {
        em_ssh_mem_deref(str);
    }
    return err;
}

/**
 *  MPInt
 **/
int em_ssh_mbuf_write_mpint(em_ssh_mbuf_t *mb, const mpz_t bn) {
    int err = OK;
    uint8_t *t = NULL;
    int bits = 0, prepend = 0;
    size_t sz =0;

    if (!mb || !bn) {
        return EINVAL;
    }
    bits = mpz_sizeinbase (bn, 2);
    sz = (bits / 8 + (bits % 8 ? 1 : 0));
    if(sz == 0) {
        err = ERROR;
        goto out;
    }

    t = em_ssh_mem_zalloc(sz, NULL);
    if(t == NULL) {
        err = ENOMEM;
        goto out;
    }
    mpz_export(t, &sz, 1, 1, 0, 0, bn);
    if (t[0] & 0x80) { prepend = 1; }
    /* size */
    em_ssh_mbuf_write_u32(mb, sz + prepend);
    /* lead zero */
    if(prepend) {
        em_ssh_mbuf_write_u8(mb, 0);
    }
    em_ssh_mbuf_write_mem(mb, t, sz);
out:
    em_ssh_mem_deref(t);
    return err;
}

/**
 *  MPInt
 **/
int em_ssh_mbuf_read_mpint(em_ssh_mbuf_t *mb, mpz_t bn) {
    int err = OK;
    size_t sz = 0, npos = 0;
    uint8_t *p = NULL;
    //
    if (!mb || !bn) {
        return EINVAL;
    }
    sz = em_ssh_mbuf_read_u32(mb);
    if(sz == 0 || (mb->pos + sz) > mb->size || sz > 16384) {
        err = ERROR;
        goto out;
    }
    npos = (mb->pos + sz);
    p = (mb->buf + mb->pos);
    if(p[0] == 0) { p++; sz--; }

    mpz_init(bn);
    mpz_import(bn, sz, 1, 1, 0, 0, p);
    em_ssh_mbuf_set_pos(mb, npos);
out:
    return err;
}

/**
 *
 **/
int em_ssh_mbuf_digest(em_ssh_mbuf_t *mb, int hash_alg, uint8_t *digest, size_t digest_len) {

    if (!mb || !digest) {
        return EINVAL;
    }

    return em_ssh_digest_memory(hash_alg, mb->buf, mb->pos, digest, digest_len);
}


