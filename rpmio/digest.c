/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"
#include "rpmio_internal.h"
#include <beecrypt/beecrypt.h>
#include <beecrypt/md5.h>
#include <beecrypt/sha1.h>
#include <beecrypt/endianness.h>
#include "debug.h"

#ifdef	SHA_DEBUG
#define	DPRINTF(_a)	fprintf _a
#else
#define	DPRINTF(_a)
#endif

/*@access DIGEST_CTX@*/

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    uint32_t datalen;		/*!< No. bytes in block of plaintext data. */
    uint32_t paramlen;		/*!< No. bytes of digest parameters. */
    uint32_t digestlen;		/*!< No. bytes of digest. */
    void * param;		/*!< Digest parameters. */
    int (*Reset) (void * param)
	/*@modifies param @*/;	/*!< Digest initialize. */
    int (*Update) (void * param, const byte * data, size_t len)
	/*@modifies param @*/;	/*!< Digest transform. */
    int (*Digest) (void * param, /*@out@*/ byte * digest)
	/*@modifies param, digest @*/;	/*!< Digest finish. */
};

DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
    nctx->param = memcpy(xcalloc(1, nctx->paramlen), octx->param, nctx->paramlen);
    return nctx;
}

DIGEST_CTX
rpmDigestInit(pgpHashAlgo hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));
    int xx;

    ctx->flags = flags;

    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	ctx->digestlen = 16;
	ctx->datalen = 64;
	/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramlen = sizeof(md5Param);
	/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramlen);
	/*@-type@*/ /* FIX: cast? */
	ctx->Reset = (void *) md5Reset;
	ctx->Update = (void *) md5Update;
	ctx->Digest = (void *) md5Digest;
	/*@=type@*/
	break;
    case PGPHASHALGO_SHA1:
	ctx->digestlen = 20;
	ctx->datalen = 64;
	/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramlen = sizeof(sha1Param);
	/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramlen);
	/*@-type@*/ /* FIX: cast? */
	ctx->Reset = (void *) sha1Reset;
	ctx->Update = (void *) sha1Update;
	ctx->Digest = (void *) sha1Digest;
	/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD160:
    case PGPHASHALGO_MD2:
    case PGPHASHALGO_TIGER192:
    case PGPHASHALGO_HAVAL_5_160:
    default:
	free(ctx);
	return NULL;
	/*@notreached@*/ break;
    }

    xx = (*ctx->Reset) (ctx->param);

DPRINTF((stderr, "*** Init(%x) ctx %p param %p\n", flags, ctx, ctx->param));
    return ctx;
}

/*@-mustmod@*/ /* LCL: ctx->param may be modified, but ctx is abstract @*/
int
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
DPRINTF((stderr, "*** Update(%p,%p,%d) param %p \"%s\"\n", ctx, data, len, ctx->param, ((char *)data)));
    return (*ctx->Update) (ctx->param, data, len);
}
/*@=mustmod@*/

int
rpmDigestFinal(/*@only@*/ DIGEST_CTX ctx, /*@out@*/ void ** datap,
	/*@out@*/ size_t *lenp, int asAscii)
{
    byte * digest;
    char * t;
    int i;

    if (ctx == NULL)
	return -1;
    digest = xmalloc(ctx->digestlen);

DPRINTF((stderr, "*** Final(%p,%p,%p,%d) param %p digest %p\n", ctx, datap, lenp, asAscii, ctx->param, digest));
    /*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Digest) (ctx->param, digest);
    /*@=noeffectuncon@*/

    /* Return final digest. */
    /*@-branchstate@*/
    if (!asAscii) {
	if (lenp) *lenp = ctx->digestlen;
	if (datap) {
	    *datap = digest;
	    digest = NULL;
	}
    } else {
	if (lenp) *lenp = (2*ctx->digestlen) + 1;
	if (datap) {
	    const byte * s = (const byte *) digest;
	    static const char hex[] = "0123456789abcdef";

	    *datap = t = xmalloc((2*ctx->digestlen) + 1);
	    for (i = 0 ; i < ctx->digestlen; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	}
    }
    /*@=branchstate@*/
    if (digest) {
	memset(digest, 0, ctx->digestlen);	/* In case it's sensitive */
	free(digest);
    }
    memset(ctx->param, 0, ctx->paramlen);	/* In case it's sensitive */
    free(ctx->param);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
    return 0;
}
