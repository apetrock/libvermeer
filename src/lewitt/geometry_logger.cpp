/*
 *  manifold_singleton.cpp
 *  Phase Vocoder
 *
 *  Created by John Delaney on 12/29/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

// #include <gl/glut.h>
#include "geometry_logger.h"

namespace lewitt
{
  namespace logger
  {
    //////////////////////////
    // debugger
    //////////////////////////

    bool geometry::instance_flag = false;
    geometry *geometry::global_instance = NULL;

    geometry &geometry::get_instance()
    {
      static geometry logger;
      if (!logger.initialized())
      {
        logger.initialized() = true;
      }

      return logger;
    }

    void geometry::point(const vec3 &p0, const vec3 &color, const float & r = 0.1)
    {
      geometry &logger = geometry::get_instance();
      logger.debugLines->add_line({p0,p0}, color, r);
    }

    void geometry::line(const vec3x2 &line,
                        const vec3 &color, const float & r = 0.1)
    {
      geometry &logger = geometry::get_instance();
      logger.debugLines->add_line(line, color, r);
    }

  } // namespace gg
}