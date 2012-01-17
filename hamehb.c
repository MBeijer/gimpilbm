#include "hamehb.h"

/**** EHB stuff ****/

grayval*
reallocEhbCmap (grayval* cmap, gint* p_ncols)
{
  gint i;
  if (VERBOSE)
    printf("Generating EHB (%d->%d) colormap entries...\n",
           *p_ncols, 2 * *p_ncols);
  cmap = g_realloc (cmap, byteppRGB * 2 * *p_ncols);
  if (!cmap) {
    g_error ("Out of memory.");
  } else {
    for (i = byteppRGB * *p_ncols - 1; i >= 0; --i) {
      cmap[byteppRGB * *p_ncols + i] = cmap[i] >> 1;
    }
    *p_ncols *= 2;
  }
  return (cmap);
}

/**** HAM stuff ****/

const guint8 hamPal[16 * byteppRGB] =
{
  0x02, 0x02, 0x24, 0x02, 0xFE, 0x44,
  0xFE, 0x02, 0x59, 0x02, 0x7E, 0x3A,
  0xFE, 0xFE, 0x49, 0x7E, 0x02, 0x42,
  0x7E, 0xFE, 0x3E, 0xFE, 0x7E, 0x85,
  0x7E, 0x7E, 0x77, 0x02, 0x02, 0xFC,
  0x02, 0xFE, 0xFC, 0xFE, 0x02, 0xFC,
  0x02, 0x7E, 0xFC, 0xFE, 0xFE, 0xFC,
  0x7E, 0x02, 0xFC, 0x7E, 0xFE, 0xFC
};

#define hamPalCols (sizeof(hamPal)/byteppRGB)

static int
judgeDiff(int r1, int g1, int b1,
          int r2, int g2, int b2)
{
  /* Is r, g, or b the smallest difference? */
  int dr, dg, db;
  int pts;
  dr = abs(r1 - r2);
  dg = abs(g1 - g2);
  db = abs(b1 - b2);
  /* ok, half its penalty */
  /* calculations are mhpf.. */
  if (dr < dg) {
    if (dr < db)
      pts = (dr >> 1) + dg + db;
    else
      pts = (db >> 1) + dr + dg;
  } else {
    if (dg < db)
      pts = (dg >> 1) + dr + db;
    else
      pts = (db >> 1) + dr + dg;
  }
  return (pts);
}

static int
judgeDiffs(guint8 * offsPtr, int r2, int g2, int b2)
{
  int bestPts = 256 * 3;
  int bestOffs = 0;             /* Make gcc happy */
  int i;
  const guchar *palPtr = hamPal + sizeof(hamPal);
  g_assert(offsPtr != NULL);
  for (i = hamPalCols - 1; i >= 0; --i) {
    int aPts;
    palPtr -= 3;
    aPts = judgeDiff(*palPtr, palPtr[1], palPtr[2], r2, g2, b2);
    if (aPts < bestPts) {
      bestOffs = i;
      bestPts = aPts;
      if (!aPts)
        break;
    }
  }
  *offsPtr = bestOffs;
  return (bestPts);
}

static void
lineToHam(guint8 * hamIdxOut, const guint8 * rgbIn, gint bytepp, gint width)
{
  /* We assume 8bit color depth per channel */
  int ar = -1, ag = 0, ab = 0;
  int nr, ng, nb;
  int crp, cgp = 0, cbp = 0;    /* Make gcc happy */
  g_assert(hamIdxOut != NULL);
  g_assert(rgbIn != NULL);
  bytepp -= 3;                  /* just need 0/1 offsets */
  while (width--) {
    int minCpts = 256 * 3;
    int constPts;
    guint8 offs;
    nr = *rgbIn++;
    ng = *rgbIn++;
    nb = *rgbIn++;
    if (ar != -1) {
      crp = judgeDiff(ar, ag, ab, (nr & 0xF0) * 17 / 16, ag, ab);
      cgp = judgeDiff(ar, ag, ab, ar, (ng & 0xF0) * 17 / 16, ab);
      cbp = judgeDiff(ar, ag, ab, ar, ag, (ab & 0xF0) * 17 / 16);
      if (crp < cgp) {
        if (crp < cbp)
          minCpts = crp;
        else
          minCpts = cbp;
      } else {
        if (cgp < cbp)
          minCpts = cgp;
        else
          minCpts = cbp;
      }
    }
    constPts = judgeDiffs(&offs, nr, ng, nb);
    if (constPts < minCpts) {
      *hamIdxOut++ = offs;
      ar = hamPal[offs * 3 + 0];
      ag = hamPal[offs * 3 + 1];
      ab = hamPal[offs * 3 + 2];
    } else {
      if (cbp == minCpts) {     /* brg 123 */
        *hamIdxOut++ = 0x10 | (nb >> 4);
        ab = (nb >> 4) * 17 / 16;
      } else if (cgp == minCpts) {
        *hamIdxOut++ = 0x30 | (ng >> 4);
        ag = (ng >> 4) * 17 / 16;
      } else {
        *hamIdxOut++ = 0x20 | (nr >> 4);
        ar = (nr >> 4) * 17 / 16;
      }
    }
    if (bytepp)
      *hamIdxOut++ = *rgbIn++;
  }
}

void
deHam(grayval * dest, const palidx * src, gint width, guint16 depth,
      const grayval * cmap, gint alpha)
{
  grayval cr = 0, cg = 0, cb = 0;
  guint8 bmask;
  g_assert(dest != NULL);
  g_assert(src != NULL);
  g_assert(width > 0);
  g_assert(depth >= 3);
  g_assert(cmap != NULL);
  depth -= 2;                   /* control bits */
  bmask = (1 << depth) - 1;
  if (width && (*src >> depth)) {
    /* FIXME: only once */
    fputs("Warning: HAM line starts with undefined color. Setting to black.\n", stderr);
  }
  while (width--) {
    guint8 idx = *src++;
    switch (idx >> depth) {     /* ham5 only */
      default:
        cr = cmap[((short) idx) * 3 + 0];
        cg = cmap[((short) idx) * 3 + 1];
        cb = cmap[((short) idx) * 3 + 2];
        break;
      case 1:
        cb = hamXbitToGray8(depth, idx & bmask);
        break;
      case 2:
        cr = hamXbitToGray8(depth, idx & bmask);
        break;
      case 3:
        cg = hamXbitToGray8(depth, idx & bmask);
        break;
    }
    *dest++ = cr;
    *dest++ = cg;
    *dest++ = cb;
    /* Leave alpha channel untouched */
    if (alpha) {
      ++dest;
    }
  }
}
