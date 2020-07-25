/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bgpencil
 */
#include <iostream>
#include <string>

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#ifdef WIN32
#  include "utfconv.h"
#endif

#include "UI_view2d.h"

#include "ED_view3d.h"

#include "gpencil_io_exporter.h"
#include "gpencil_io_svg.h"

#include "pugixml.hpp"

namespace blender {
namespace io {
namespace gpencil {

/* Constructor. */
GpencilExporterSVG::GpencilExporterSVG(const struct GpencilExportParams *params)
{
  this->params.frame_start = params->frame_start;
  this->params.frame_end = params->frame_end;
  this->params.ob = params->ob;
  this->params.region = params->region;
  this->params.C = params->C;
  this->params.filename = params->filename;
  this->params.mode = params->mode;

  /* Prepare output filename with full path. */
  set_out_filename(params->C, params->filename);
}

/* Main write method for SVG format. */
bool GpencilExporterSVG::write(void)
{
  create_document_header();
  layers_loop();

  doc.save_file(out_filename);

  return true;
}

/* Create document header and main svg node. */
void GpencilExporterSVG::create_document_header(void)
{
  int x = params.region->winx;
  int y = params.region->winy;

  /* Add a custom document declaration node */
  pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";

  pugi::xml_node comment = doc.append_child(pugi::node_comment);
  comment.set_value(" Generator: Blender, SVG Export for Grease Pencil ");

  pugi::xml_node doctype = doc.append_child(pugi::node_doctype);
  doctype.set_value(
      "svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" "
      "\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\"");

  main_node = doc.append_child("svg");
  main_node.append_attribute("version").set_value("1.0");
  main_node.append_attribute("x").set_value("0px");
  main_node.append_attribute("y").set_value("0px");

  std::string width = std::to_string(x) + "px";
  std::string height = std::to_string(y) + "px";
  main_node.append_attribute("width").set_value(width.c_str());
  main_node.append_attribute("height").set_value(height.c_str());
  std::string viewbox = "0 0 " + std::to_string(x) + " " + std::to_string(y);
  main_node.append_attribute("viewBox").set_value(viewbox.c_str());
}

/* Main layer loop. */
void GpencilExporterSVG::layers_loop(void)
{
  float color[3] = {1.0f, 0.5f, 0.01f};
  std::string hex = rgb_to_hex(color);

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(params.C);
  Object *ob = params.ob;

  bGPdata *gpd = (bGPdata *)ob->data;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Layer node. */
    std::string txt = "Layer: ";
    txt.append(gpl->info);
    main_node.append_child(pugi::node_comment).set_value(txt.c_str());
    pugi::xml_node gpl_node = main_node.append_child("g");
    gpl_node.append_attribute("id").set_value(gpl->info);

    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) {
      continue;
    }

    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      float diff_mat[4][4];
      BKE_gpencil_parent_matrix_get(depsgraph, ob, gpl, diff_mat);

      pugi::xml_node gps_node = gpl_node.append_child("path");
      // gps_node.append_attribute("fill").set_value("#000000");
      gps_node.append_attribute("stroke").set_value("#000000");
      gps_node.append_attribute("stroke-width").set_value("1.0");
      std::string txt = "M";
      for (int i = 0; i < gps->totpoints; i++) {
        if (i > 0) {
          txt.append("L");
        }
        bGPDspoint *pt = &gps->points[i];
        int screen_co[2];
        gpencil_3d_point_to_screen_space(params.region, diff_mat, &pt->x, screen_co);
        /* Invert Y axis. */
        int y = params.region->winy - screen_co[1];
        txt.append(std::to_string(screen_co[0]) + "," + std::to_string(y));
      }
      /* Close patch (cyclic)*/
      if (gps->flag & GP_STROKE_CYCLIC) {
        txt.append("z");
      }

      gps_node.append_attribute("d").set_value(txt.c_str());
    }
  }
}

}  // namespace gpencil
}  // namespace io
}  // namespace blender
