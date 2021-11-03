add_subdirectory(inline)
add_subdirectory(share)
add_subdirectory(template)
add_subdirectory(zfp)

set(SOURCES ${SOURCES}
  zfp.c zfp.h
  bitstream.c bitstream.h
        zfp/macros.h zfp/system.h zfp/types.h zfp/version.h
  traitsf.h traitsd.h traitsi.h traitsl.h
  block1.h block2.h block3.h block4.h
  encode1f.c encode1d.c encode1i.c encode1l.c
  decode1f.c decode1d.c decode1i.c decode1l.c
  encode2f.c encode2d.c encode2i.c encode2l.c
  decode2f.c decode2d.c decode2i.c decode2l.c
  encode3f.c encode3d.c encode3i.c encode3l.c
  decode3f.c decode3d.c decode3i.c decode3l.c
  encode4f.c encode4d.c encode4i.c encode4l.c
  decode4f.c decode4d.c decode4i.c decode4l.c)

