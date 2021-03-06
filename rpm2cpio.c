/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "rpmlib.h"
#include "debug.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    Header h;
    char * rpmio_flags;
    const int * archive_size;
    int rc, isSource;
    FD_t gzdi;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1)
	fdi = fdDup(STDIN_FILENO);
    else
	fdi = Fopen(argv[1], "r.ufdio");

    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);

    rc = rpmReadPackageHeader(fdi, &h, &isSource, NULL, NULL);
    switch (rc) {
    case 0:
	break;
    case 1:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    default:
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
	break;
    }

    /* Retrieve type of payload compression. */
    {	const char * payload_compressor = NULL;
	char * t;

	if (!headerGetEntry(h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	rpmio_flags = t = alloca(sizeof("r.gzdio"));
	*t++ = 'r';
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
	if (!strcmp(payload_compressor, "lzma"))
	    t = stpcpy(t, ".lzdio");
	if (!strcmp(payload_compressor, "xz"))
	    t = stpcpy(t, ".xzdio");

	if (!headerGetEntry(h, RPMTAG_ARCHIVESIZE, NULL,
			    (void **) &archive_size, NULL))
	    archive_size = NULL;
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    if (gzdi == NULL) {
	fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
	exit(EXIT_FAILURE);
    }

    rc = ufdCopy(gzdi, fdo);
    rc = archive_size
	    ? (*archive_size != rc ? EXIT_FAILURE : EXIT_SUCCESS)
	    : ((rc <= 0) ? EXIT_FAILURE : EXIT_SUCCESS);
    if (Fclose(fdo) < 0)
	rc = EXIT_FAILURE;

    if (Fclose(gzdi) < 0)	/* XXX gzdi == fdi */
	rc = EXIT_FAILURE;

    return rc;
}
