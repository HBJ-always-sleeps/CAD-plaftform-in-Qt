# -*- coding: utf-8 -*-
"""HydraulicCADPlatform v4.2 geology top-view entry point.

The verified section-by-section implementation is kept in a separate module
so the same code is used by both platform runs and independent diagnostics.
"""

from geology_topview_section_connect_debug import main


if __name__ == "__main__":
    raise SystemExit(main())
