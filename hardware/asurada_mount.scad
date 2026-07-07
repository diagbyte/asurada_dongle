// =============================================================================
// Asurada dongle - parametric mechanical core (OpenSCAD)
//
// This is the FUNCTIONAL core only: a front bezel that holds the round GC9A01
// LCD, a tray that holds the Seeed XIAO nRF52840, standoffs that stack them,
// and an outer "registration ring" that the sculpted Asurada head shell mates
// to (model the organic head in Blender/Fusion around this ring).
//
// Units: millimetres.  Dimensions marked (!) are best-guess defaults you MUST
// verify against your actual parts (Waveshare 1.28" module + XIAO) before
// printing - measure with calipers or pull the Waveshare mechanical drawing.
//
// Quick use:
//   set `part` below, then Render (F6) and Export STL. `part = "assembly"`
//   shows everything together (exploded) for a sanity check.
// =============================================================================

part = "assembly";   // "assembly" | "bezel" | "tray" | "shell_ring"

$fn = 120;

// ---------- Display: Waveshare 1.28" round LCD (GC9A01, 240x240) ----------
disp_active_dia = 32.4;   // visible active area (240 px @ ~0.135 mm)
disp_glass_dia  = 35.4;   // (!) round cover-glass diameter to clear
disp_board_dia  = 37.5;   // (!) round PCB diameter that rests on the shelf
disp_board_th   = 1.6;    // (!) PCB thickness
disp_stack_th   = 2.0;    // (!) glass+panel height standing above the PCB

// ---------- MCU: Seeed XIAO nRF52840 ----------
xiao_l      = 21.0;       // length
xiao_w      = 17.8;       // width
xiao_pcb_th = 1.2;        // PCB thickness
xiao_usb_w  = 9.2;        // (!) USB-C connector cutout width
xiao_usb_h  = 3.6;        // (!) USB-C connector cutout height
xiao_usb_over = 1.2;      // how far the USB-C overhangs the board edge

// ---------- Structure / fit ----------
tol         = 0.30;       // general clearance
front_th    = 2.0;        // bezel face thickness (in front of the glass)
bezel_rim   = 3.2;        // visible rim around the glass
wall        = 2.6;        // general wall thickness
standoff_h  = 6.5;        // gap between LCD back and the XIAO tray
tray_th     = 2.4;        // tray floor thickness
screw_d     = 2.2;        // M2 self-tapping pilot
boss_d      = 5.2;        // screw boss outer diameter
post_n      = 3;          // number of standoff posts

// ---------- Outer shell registration ring ----------
shell_wall  = 2.6;        // wall thickness of the ring the head shell grips
shell_lip_h = 4.0;        // height of the mating lip

// ---------- Derived ----------
body_dia    = disp_board_dia + 2*tol + 2*wall;   // outer diameter of the core
glass_pocket_dia = disp_glass_dia + 2*tol;
board_pocket_dia = disp_board_dia + 2*tol;
aperture_dia = disp_active_dia + 1.2;            // slightly larger than active
post_r      = disp_board_dia/2 - 1.5;            // radius the posts sit on
eps         = 0.01;

// =============================================================================
// Modules
// =============================================================================

// Ring of screw posts / standoffs between bezel and tray.
module posts(h, outer_d, hole_d) {
    for (i = [0 : post_n - 1]) {
        a = i * 360 / post_n + 60;   // offset so a post clears the USB edge
        rotate([0, 0, a])
            translate([post_r, 0, 0])
                difference() {
                    cylinder(h = h, d = outer_d);
                    translate([0, 0, -eps])
                        cylinder(h = h + 2*eps, d = hole_d);
                }
    }
}

// FRONT BEZEL: face with the round aperture + a shelf/pocket the LCD sits in
// from behind. LCD glass points forward through the aperture.
module bezel() {
    difference() {
        union() {
            // outer body
            cylinder(h = front_th + disp_stack_th + disp_board_th + tol, d = body_dia);
        }
        // viewing aperture (through the front face)
        translate([0, 0, -eps])
            cylinder(h = front_th + 2*eps, d = aperture_dia);
        // glass pocket (recess for the cover glass)
        translate([0, 0, front_th])
            cylinder(h = disp_stack_th + tol, d = glass_pocket_dia);
        // board pocket (recess for the PCB, larger)
        translate([0, 0, front_th + disp_stack_th])
            cylinder(h = disp_board_th + tol + eps, d = board_pocket_dia);
    }
    // standoff posts grow rearward from the bezel back face
    translate([0, 0, front_th + disp_stack_th + disp_board_th + tol])
        posts(standoff_h, boss_d, screw_d);
}

// XIAO TRAY: floor with a pocket for the XIAO and a USB-C slot on one edge.
// Screws pass up through the posts into the bezel (or the tray receives them).
module tray() {
    difference() {
        cylinder(h = tray_th, d = body_dia);
        // XIAO pocket
        translate([-xiao_l/2, -xiao_w/2, tray_th - xiao_pcb_th - tol])
            cube([xiao_l + 2*tol, xiao_w + 2*tol, xiao_pcb_th + tol + eps]);
        // USB-C slot through the wall on the +X short edge
        translate([xiao_l/2 - xiao_usb_over, -xiao_usb_w/2, tray_th - xiao_pcb_th/2 - xiao_usb_h/2])
            cube([body_dia, xiao_usb_w, xiao_usb_h]);
    }
    // matching screw bosses (solid, self-tap) aligned under the posts
    for (i = [0 : post_n - 1]) {
        a = i * 360 / post_n + 60;
        rotate([0, 0, a])
            translate([post_r, 0, tray_th - eps])
                difference() {
                    cylinder(h = 2.5, d = boss_d);
                    translate([0, 0, -eps]) cylinder(h = 3.0, d = screw_d - 0.4);
                }
    }
}

// OUTER SHELL REGISTRATION RING: the sculpted Asurada head shell wraps this.
// It is a simple lipped ring concentric with the display; import it into your
// CAD/sculpt tool as the datum for the head's inner cavity.
module shell_ring() {
    difference() {
        cylinder(h = shell_lip_h, d = body_dia + 2*shell_wall);
        translate([0, 0, -eps])
            cylinder(h = shell_lip_h + 2*eps, d = body_dia + 2*tol);
    }
}

// =============================================================================
// Layout
// =============================================================================
module assembly() {
    color("DimGray") bezel();
    // tray sits behind the bezel, exploded downward for visualization
    color("SteelBlue")
        translate([0, 0, -(standoff_h + tray_th + 8)])
            tray();
    // shell ring around the body
    color("Goldenrod")
        translate([0, 0, -2])
            shell_ring();
}

if (part == "assembly") assembly();
else if (part == "bezel") bezel();
else if (part == "tray") tray();
else if (part == "shell_ring") shell_ring();
