/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2011 Róman Joost <romanofski@gimp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "xmp-parse.h"
#include "xmp-encode.h"
#include "xmp-model.h"


#define ADD_TEST(function) \
  g_test_add ("/metadata-xmp-model/" #function, \
              GimpTestFixture, \
              NULL, \
              gimp_test_xmp_model_setup, \
              function, \
              gimp_test_xmp_model_teardown);


typedef struct
{
  XMPModel *xmp_model;
} GimpTestFixture;


typedef struct
{
  const gchar *schema_name;
  const gchar *name;
  int          pos;
} TestDataEntry;

static TestDataEntry propertiestotest[] =
{
   { XMP_PREFIX_DUBLIN_CORE,  "title",          1 },
   { XMP_PREFIX_DUBLIN_CORE,  "creator",        0 },
   { XMP_PREFIX_DUBLIN_CORE,  "description",    1 },
   { XMP_PREFIX_PHOTOSHOP,    "CaptionWriter",  0 },
   { NULL,  NULL,          0 }
};
TestDataEntry * const import_exportdata = propertiestotest;


static void gimp_test_xmp_model_setup       (GimpTestFixture *fixture,
                                             gconstpointer    data);
static void gimp_test_xmp_model_teardown    (GimpTestFixture *fixture,
                                             gconstpointer    data);


/**
 * gimp_test_xmp_model_setup:
 * @fixture: GimpTestFixture fixture
 * @data:
 *
 * Test fixture to setup an XMPModel.
 **/
static void
gimp_test_xmp_model_setup (GimpTestFixture *fixture,
                           gconstpointer    data)
{
  fixture->xmp_model = xmp_model_new ();
}


static void
gimp_test_xmp_model_teardown (GimpTestFixture *fixture,
                              gconstpointer    data)
{
  g_object_unref (fixture->xmp_model);
}

/**
 * test_xmp_model_import_export_structures:
 * @fixture:
 * @data:
 *
 * Test to assure the round trip of data import, editing, export is
 * working.
 **/
static void
test_xmp_model_import_export_structures (GimpTestFixture *fixture,
                                         gconstpointer    data)
{
  int             i, j;
  gboolean        result;
  const gchar   **before_value;
  const gchar   **after_value;
  GString        *buffer;
  TestDataEntry  *testdata;
  const gchar    *scalarvalue = "test";
  GError        **error       = NULL;
  gchar          *uri         = NULL;


  uri = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                          "plug-ins/metadata/tests/files/test.xmp",
                          NULL);

  xmp_model_parse_file (fixture->xmp_model, uri, error);
  g_free (uri);

  for (i = 0; import_exportdata[i].name != NULL; ++i)
   {
    testdata = &(import_exportdata[i]);

    /* backup the original raw value */
    before_value = xmp_model_get_raw_property_value (fixture->xmp_model,
                                                     testdata->schema_name,
                                                     testdata->name);
    g_assert (before_value != NULL);

    /* set a new scalar value */
    result = xmp_model_set_scalar_property (fixture->xmp_model,
                                            testdata->schema_name,
                                            testdata->name,
                                            scalarvalue);
    g_assert (result == TRUE);

    /* export */
    buffer = g_string_new ("GIMP_TEST");
    xmp_generate_packet (fixture->xmp_model, buffer);

    /* import */
    xmp_model_parse_buffer (fixture->xmp_model,
                            buffer->str,
                            buffer->len,
                            TRUE,
                            error);
    after_value = xmp_model_get_raw_property_value (fixture->xmp_model,
                                                    testdata->schema_name,
                                                    testdata->name);
    /* check that the scalar value is correctly exported */
    g_assert (after_value != NULL);
    g_assert_cmpstr (after_value[testdata->pos], ==, scalarvalue);

    /* check that the original data is not changed */
    for (j = 0; after_value[j] != NULL; ++j)
     {
       if (j == testdata->pos)
         continue;

       g_assert (before_value[j] != NULL);
       g_assert (after_value[j]  != NULL);
       g_assert_cmpstr (before_value[j], ==, after_value[j]);
     }
   }
}

/**
 * test_xmp_model_import_export:
 * @fixture:
 * @data:
 *
 * Functional test, which assures that changes in the string
 * representation is correctly merged on export. This test starts of
 * with inserting scalar values only.
 **/
static void
test_xmp_model_import_export (GimpTestFixture *fixture,
                              gconstpointer    data)
{
  gboolean        result;
  GString        *buffer;
  TestDataEntry  *testdata;
  const gchar   **after_value;
  const gchar    *scalarvalue = "test";
  GError        **error       = NULL;

  /* dc:title */
  testdata = &(import_exportdata[0]);

  /* set a new scalar value */
  result = xmp_model_set_scalar_property (fixture->xmp_model,
                                          testdata->schema_name,
                                          testdata->name,
                                          scalarvalue);
  g_assert (result == TRUE);

  /* export */
  buffer = g_string_new ("GIMP_TEST");
  xmp_generate_packet (fixture->xmp_model, buffer);

  /* import */
  xmp_model_parse_buffer (fixture->xmp_model,
                          buffer->str,
                          buffer->len,
                          TRUE,
                          error);
  after_value = xmp_model_get_raw_property_value (fixture->xmp_model,
                                                  testdata->schema_name,
                                                  testdata->name);

  /* check that the scalar value is correctly exported */
  g_assert (after_value != NULL);
  g_assert_cmpstr (after_value[testdata->pos], ==, scalarvalue);
}

int main(int argc, char **argv)
{
  gint result = -1;

  g_type_init();
  g_test_init (&argc, &argv, NULL);

  ADD_TEST (test_xmp_model_import_export);
  ADD_TEST (test_xmp_model_import_export_structures);

  result = g_test_run ();

  return result;
}