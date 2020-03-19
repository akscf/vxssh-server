/**
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "emssh.h"
#define RSA_TYPE_NAME "ssh-rsa"

/**
 *
 **/
int em_ssh_rsa_encode_public_key(em_ssh_mbuf_t *mb, em_ssh_crypto_rsa_public_key_t *key) {
    int err = OK;

    if(!mb || !key) {
        return EINVAL;
    }
    /* type */
    err = em_ssh_mbuf_write_str_sz(mb, RSA_TYPE_NAME);
    if(err != OK) {
        return err;
    }
    /* rsa e */
    err = em_ssh_mbuf_write_mpint(mb, key->e);
    if(err != OK) {
        return err;
    }
    /* rsa n */
    err = em_ssh_mbuf_write_mpint(mb, key->n);
    if(err != OK) {
        return err;
    }

    return err;
}

/**
 * from private key
 **/
int em_ssh_rsa_encode_public_key2(em_ssh_mbuf_t *mb, em_ssh_crypto_rsa_private_key_t *key) {
    int err = OK;

    if(!mb || !key) {
        return EINVAL;
    }
    /* type */
    err = em_ssh_mbuf_write_str_sz(mb, RSA_TYPE_NAME);
    if(err != OK) {
        return err;
    }
    /* rsa e */
    err = em_ssh_mbuf_write_mpint(mb, key->e);
    if(err != OK) {
        return err;
    }
    /* rsa n */
    err = em_ssh_mbuf_write_mpint(mb, key->n);
    if(err != OK) {
        return err;
    }

    return err;
}

/**
 *
 **/
int em_ssh_rsa_decode_public_key(em_ssh_mbuf_t *mb, em_ssh_crypto_rsa_public_key_t *key) {
    int err = OK;
    char *tbuf = NULL;
    size_t klen = 0, sz = 0;

    if(!mb || !key) {
        return EINVAL;
    }
    /* key len */
    klen = em_ssh_mbuf_read_u32(mb);
    if(klen == 0 || klen > mb->size) {
        err = ERROR; goto out;
    }
    /* type */
    sz = em_ssh_mbuf_read_u32(mb);
    if(sz == 0 || sz > klen) {
        err = ERROR; goto out;
    }
    if((tbuf = em_ssh_mem_zalloc(sz, NULL)) == NULL) {
        err = ENOMEM; goto out;
    }
    if((err = em_ssh_mbuf_read_mem(mb, (uint8_t *)tbuf, &sz)) != OK) {
        goto out;
    }
    if(sz != strlen(RSA_TYPE_NAME) || strncmp(RSA_TYPE_NAME, tbuf, strlen(RSA_TYPE_NAME)) != 0) {
        em_ssh_log_error("RSA: invalid key type");
        err = ERROR; goto out;
    }
    /* rsa e */
    err = em_ssh_mbuf_read_mpint(mb, key->e);
    if(err != OK) {
        return err;
    }
    /* rsa n */
    err = em_ssh_mbuf_read_mpint(mb, key->n);
    if(err != OK) {
        return err;
    }
out:
    em_ssh_mem_deref(tbuf);
    return err;
}

/**
 *
 **/
int em_ssh_rsa_encode_signature(em_ssh_mbuf_t *mb, em_ssh_crypto_rsa_signature_t *sign) {
    int err = OK, bits = 0;
    uint8_t *tbuf = NULL;
    size_t sz =0;

    if(!mb || !sign) {
        return EINVAL;
    }

    bits = mpz_sizeinbase (sign->s, 2);
    sz = (bits / 8 + (bits % 8 ? 1 : 0));
    if(sz == 0) {
        err = ERROR; goto out;
    }
    if((tbuf = em_ssh_mem_zalloc(sz, NULL)) == NULL) {
        err = ENOMEM; goto out;
    }
    /* type */
    err = em_ssh_mbuf_write_str_sz(mb, RSA_TYPE_NAME);
    if(err != OK) {
        goto out;
    }
    /* rsa s */
    mpz_export(tbuf, &sz, 1, 1, 0, 0, sign->s);
    if (tbuf[0] == 0x0) {
        em_ssh_mbuf_write_u32(mb, sz - 1);
        em_ssh_mbuf_write_mem(mb, tbuf + 1, sz - 1);
    } else {
        em_ssh_mbuf_write_u32(mb, sz);
        em_ssh_mbuf_write_mem(mb, tbuf, sz);
    }
out:
    em_ssh_mem_deref(tbuf);
    return err;
}

/**
 *
 **/
int em_ssh_rsa_decode_signature(em_ssh_mbuf_t *mb, em_ssh_crypto_rsa_signature_t *sign) {
    int err = OK;
    char *tbuf = NULL;
    size_t slen = 0, sz = 0;

    if(!mb || !sign) {
        return EINVAL;
    }
    /* sign len */
    slen = em_ssh_mbuf_read_u32(mb);
    if(slen == 0 || slen > mb->size) {
        err = ERROR; goto out;
    }
    /* type */
    sz = em_ssh_mbuf_read_u32(mb);
    if(sz == 0 || sz > slen) {
        err = ERROR; goto out;
    }
    if((tbuf = em_ssh_mem_zalloc(sz, NULL)) == NULL) {
        err = ENOMEM; goto out;
    }
    if((err = em_ssh_mbuf_read_mem(mb, (uint8_t *)tbuf, &sz)) != OK) {
        goto out;
    }
    if(sz != strlen(RSA_TYPE_NAME) || strncmp(RSA_TYPE_NAME, tbuf, strlen(RSA_TYPE_NAME)) != 0) {
        em_ssh_log_error("RSA: invalid sign type");
        err = ERROR; goto out;
    }
    /* rsa s */
    err = em_ssh_mbuf_read_mpint(mb, sign->s);
    if(err != OK) {
        goto out;
    }
out:
    em_ssh_mem_deref(tbuf);
    return err;
}

/**
* sing the data
**/
int em_ssh_rsa_sign(em_ssh_crypto_rsa_private_key_t *key, const uint8_t *data, size_t data_len, em_ssh_crypto_object_t **signature)  {
    static const uint8_t hdr[] = { 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };
    uint8_t digest[EM_SSH_DIGEST_SHA1_LENGTH];
    size_t digest_len = EM_SSH_DIGEST_SHA1_LENGTH;
    int err = OK, i=0;
    size_t block_len, pad_len;
    em_ssh_crypto_object_t *sigobj=NULL;
    em_ssh_crypto_rsa_signature_t *sigref=NULL;
    em_ssh_mbuf_t *sigmb = NULL;
    mpz_t m;

    if(!key || !data || !signature) {
        return EINVAL;
    }
    err = em_ssh_crypto_object_alloc(&sigobj, CRYPTO_OBJECT_RSA_SIGNATURE);
    if(err != OK) {
        goto out;
    }
    sigref = (em_ssh_crypto_rsa_signature_t *) sigobj->obj;

    if((err = em_ssh_digest_memory(EM_SSH_DIGEST_SHA1, data, data_len, digest, digest_len)) != OK) {
        goto out;
    }
    block_len = ((mpz_sizeinbase(key->n, 2) + 7) / 8);
    pad_len = block_len - (sizeof(hdr) + digest_len + 2) - 1;
    if (pad_len < 8) {
        em_ssh_log_error("RSA: message too long");
        err = ERROR; goto out;
    }

    if((err = em_ssh_mbuf_alloc(&sigmb, (sizeof(hdr) + digest_len + pad_len + 2))) != OK) {
        goto out;
    }
    if((err = em_ssh_mbuf_write_u8(sigmb, 0x1)) != OK) {
        goto out;
    }
    if((err = em_ssh_mbuf_fill(sigmb, 0xff, pad_len)) != OK) {
        goto out;
    }
    if((err = em_ssh_mbuf_write_u8(sigmb, 0x0)) != OK) {
        goto out;
    }
    if((err = em_ssh_mbuf_write_mem(sigmb, hdr, sizeof(hdr))) != OK) {
        goto out;
    }
    if((err = em_ssh_mbuf_write_mem(sigmb, digest, digest_len)) != OK) {
        goto out;
    }
    mpz_init(m);
    mpz_import(m, sigmb->pos, 1, 1, 0, 0, sigmb->buf);
    mpz_powm(sigref->s, m, key->d, key->n);
out:
    if(err != OK) {
        em_ssh_mem_deref(sigobj);
    } else {
        *signature = sigobj;
    }
    mpz_clear(m);
    explicit_bzero(digest, EM_SSH_DIGEST_SHA1_LENGTH);
    em_ssh_mem_deref(sigmb);
    return err;
}

/**
 *
 **/
int em_ssh_rsa_sign_verfy(em_ssh_crypto_rsa_public_key_t *key, em_ssh_crypto_object_t *signature, const uint8_t *data, size_t data_len)  {

    if(!key || !data || !signature) {
        return EINVAL;
    }
    /*
    * todo
    */
    return ENOSYS;
}
