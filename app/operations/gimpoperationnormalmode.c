/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpoperationnormalmode.c
 * Copyright (C) 2012 Michael Natterer <mitch@gimp.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gegl-plugin.h>

#include <libgimpbase/gimpbase.h>

#include "operations-types.h"

#include "gimpoperationnormalmode.h"

GimpLayerModeFunction gimp_operation_normal_mode_process_pixels = NULL;


static gboolean gimp_operation_normal_parent_process (GeglOperation        *operation,
                                                      GeglOperationContext *context,
                                                      const gchar          *output_prop,
                                                      const GeglRectangle  *result,
                                                      gint                  level);
static gboolean gimp_operation_normal_mode_process   (GeglOperation        *operation,
                                                      void                 *in_buf,
                                                      void                 *aux_buf,
                                                      void                 *aux2_buf,
                                                      void                 *out_buf,
                                                      glong                 samples,
                                                      const GeglRectangle  *roi,
                                                      gint                  level);


G_DEFINE_TYPE (GimpOperationNormalMode, gimp_operation_normal_mode,
               GIMP_TYPE_OPERATION_POINT_LAYER_MODE)

#define parent_class gimp_operation_normal_mode_parent_class

static const gchar* reference_xml = "<?xml version='1.0' encoding='UTF-8'?>"
"<gegl>"
"<node operation='gimp:normal-mode'>"
"  <node operation='gegl:load'>"
"    <params>"
"      <param name='path'>blending-test-B.png</param>"
"    </params>"
"  </node>"
"</node>"
"<node operation='gegl:load'>"
"  <params>"
"    <param name='path'>blending-test-A.png</param>"
"  </params>"
"</node>"
"</gegl>";


static void
gimp_operation_normal_mode_class_init (GimpOperationNormalModeClass *klass)
{
  GeglOperationClass               *operation_class;
  GeglOperationPointComposer3Class *point_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  point_class     = GEGL_OPERATION_POINT_COMPOSER3_CLASS (klass);

  gegl_operation_class_set_keys (operation_class,
                                 "name",                  "gimp:normal-mode",
                                 "description",           "GIMP normal mode operation",
                                 "reference-image",       "normal-mode.png",
                                 "reference-composition", reference_xml,
                                 NULL);

  operation_class->process     = gimp_operation_normal_parent_process;

  point_class->process         = gimp_operation_normal_mode_process;

  gimp_operation_normal_mode_process_pixels = gimp_operation_normal_mode_process_pixels_core;

#if COMPILE_SSE2_INTRINISICS
  if (gimp_cpu_accel_get_support() & GIMP_CPU_ACCEL_X86_SSE2)
    gimp_operation_normal_mode_process_pixels = gimp_operation_normal_mode_process_pixels_sse2;
#endif /* COMPILE_SSE2_INTRINISICS */

#if COMPILE_SSE4_1_INTRINISICS
  if (gimp_cpu_accel_get_support() & GIMP_CPU_ACCEL_X86_SSE4_1)
    gimp_operation_normal_mode_process_pixels = gimp_operation_normal_mode_process_pixels_sse4;
#endif /* COMPILE_SSE4_1_INTRINISICS */
}

static void
gimp_operation_normal_mode_init (GimpOperationNormalMode *self)
{
}

static gboolean
gimp_operation_normal_parent_process (GeglOperation        *operation,
                                      GeglOperationContext *context,
                                      const gchar          *output_prop,
                                      const GeglRectangle  *result,
                                      gint                  level)
{
  GimpOperationPointLayerMode *point;

  point = GIMP_OPERATION_POINT_LAYER_MODE (operation);

  if (point->opacity == 1.0 &&
      ! gegl_operation_context_get_object (context, "aux2"))
    {
      const GeglRectangle *in_extent  = NULL;
      const GeglRectangle *aux_extent = NULL;
      GObject             *input;
      GObject             *aux;

      /* get the raw values this does not increase the reference count */
      input = gegl_operation_context_get_object (context, "input");
      aux   = gegl_operation_context_get_object (context, "aux");

      /* pass the input/aux buffers directly through if they are not
       * overlapping
       */
      if (input)
        in_extent = gegl_buffer_get_abyss (GEGL_BUFFER (input));

      if (! input ||
          (aux && ! gegl_rectangle_intersect (NULL, in_extent, result)))
        {
          gegl_operation_context_set_object (context, "output", aux);
          return TRUE;
        }

      if (aux)
        aux_extent = gegl_buffer_get_abyss (GEGL_BUFFER (aux));

      if (! aux ||
          (input && ! gegl_rectangle_intersect (NULL, aux_extent, result)))
        {
          gegl_operation_context_set_object (context, "output", input);
          return TRUE;
        }
    }

  /* chain up, which will create the needed buffers for our actual
   * process function
   */
  return GEGL_OPERATION_CLASS (parent_class)->process (operation, context,
                                                       output_prop, result,
                                                       level);
}

static gboolean
gimp_operation_normal_mode_process (GeglOperation       *operation,
                                    void                *in_buf,
                                    void                *aux_buf,
                                    void                *aux2_buf,
                                    void                *out_buf,
                                    glong                samples,
                                    const GeglRectangle *roi,
                                    gint                 level)
{
  gfloat opacity = GIMP_OPERATION_POINT_LAYER_MODE (operation)->opacity;

  return gimp_operation_normal_mode_process_pixels (in_buf, aux_buf, aux2_buf, out_buf, opacity, samples, roi, level);
}

gboolean
gimp_operation_normal_mode_process_pixels_core (gfloat              *in,
                                                gfloat              *aux,
                                                gfloat              *mask,
                                                gfloat              *out,
                                                gfloat               opacity,
                                                glong                samples,
                                                const GeglRectangle *roi,
                                                gint                 level)
{
  const gboolean has_mask = mask != NULL;

  while (samples--)
    {
      gfloat aux_alpha;

      aux_alpha = aux[ALPHA] * opacity;
      if (has_mask)
        aux_alpha *= *mask;

      out[ALPHA] = aux_alpha + in[ALPHA] - aux_alpha * in[ALPHA];

      if (out[ALPHA])
        {
          gfloat in_weight       = in[ALPHA] * (1.0f - aux_alpha);
          gfloat recip_out_alpha = 1.0f / out[ALPHA];
          gint   b;

          for (b = RED; b < ALPHA; b++)
            {
              out[b] = (aux[b] * aux_alpha + in[b] * in_weight) * recip_out_alpha;
            }
        }
      else
        {
          gint b;

          for (b = RED; b < ALPHA; b++)
            {
              out[b] = in[b];
            }
        }

      in   += 4;
      aux  += 4;
      out  += 4;

      if (has_mask)
        mask++;
    }

  return TRUE;
}
