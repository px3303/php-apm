PHP_ARG_ENABLE(hcapm, whether to enable hcapm support,
[ --enable-hcapm   Enable hcapm support])

if test "$PHP_HCAPM" != "no"; then
  AC_DEFINE(HAVE_HCAPM, 1, [ Have hcapm support ])

  PHP_NEW_EXTENSION(hcapm, hcapm.c cJSON.c, $ext_shared)
fi
