$fs = 0.1;
$fa = 2;

use <./Round-Anything/polyround.scad>

module roundedBaseCube(dim, r){
    linear_extrude(dim[2]) polygon(polyRound([[0, 0, r], [0, dim[1], r], [dim[0], dim[1], r], [dim[0], 0, r]]));
}

base_height = 3;

motor_thickness = 3;
motor_pos = [42, 78];
motor_height = 16;
motor_top_height = 1;

trafo_thickness = 3;
trafo_pos = [20, 7];
trafo_size = [53.5, 44.5];
trafo_height = 7;
trafo_top_height = 1;

button_thickness = 2;
button_height = 20;

triang_corners = [
    [-6 * (1 + cos(71.368))/sin(71.368), -6],
    [104, -6],
    [36., 130, 0]
];

button_corners = [
    [triang_corners[1][0] + sin(36)*75,               triang_corners[1][1] + cos(36)*75],
    [triang_corners[1][0] + sin(36)*75 + sin(-54)*19, triang_corners[1][1] + cos(36)*75 + cos(-54)*19],
    [triang_corners[1][0] + sin(36)*20 + sin(-54)*19, triang_corners[1][1] + cos(36)*20 + cos(-54)*19],
];

difference(){
    union(){
        linear_extrude(height = base_height){
            difference(){
                polygon(polyRound([
                    [triang_corners[0][0], triang_corners[0][1], 6],
                    [triang_corners[1][0], triang_corners[1][1], 6],

                    [button_corners[0][0], button_corners[0][1], 0],
                    [button_corners[1][0], button_corners[1][1], 0],
                    [button_corners[2][0], button_corners[2][1], 6],

                    [triang_corners[2][0], triang_corners[2][1], 6]
                ]));
                translate([0, 0]) circle(r = 3);
                translate([100, 0]) circle(r = 3);
                translate([37.7, 111.8]) circle(r = 6);
                translate([37.7, 111.8 + 50]) circle(r = 48);
            }
        }
        translate([motor_pos[0], motor_pos[1], -motor_height - motor_top_height])
            cylinder(
                h = base_height + motor_height + motor_top_height,
                r1=motor_thickness + 20,
                r2=motor_thickness + 20);
        translate([trafo_pos[0] - trafo_thickness, trafo_pos[1] - trafo_thickness, -trafo_height - trafo_top_height])
            cube([
                trafo_size[0] + 2*trafo_thickness,
                trafo_size[1] + 2*trafo_thickness,
                base_height + trafo_height + trafo_top_height]);
        translate([button_corners[0][0], button_corners[0][1], -button_height + base_height])
            rotate([0, 0, 54])
                difference(){
                    rotate([0, -90, 0]) roundedBaseCube([button_height, 19, button_thickness], 3);
                    translate([-10, 3, 3]) rotate([90, 0, 90]) roundedBaseCube([3, button_height - 6, 100], 1.5);
                    translate([-10, 3 + 10, 3]) rotate([90, 0, 90]) roundedBaseCube([3, button_height - 6, 100], 1.5);
                }
    }
    translate([motor_pos[0], motor_pos[1], -motor_height])
        cylinder(
            h = base_height + 1 + motor_height,
            r1 = 20,
            r2 = 20
        );
    translate([motor_pos[0], motor_pos[1], -200])
        cylinder(
            h = 195,
            r1 = 8,
            r2 = 8
        );
    translate([motor_pos[0] + 13, motor_pos[1], -200])
        cylinder(
            h = 195,
            r1 = 1.125,
            r2 = 1.125
        );
    translate([motor_pos[0], motor_pos[1] + 13, -200])
        cylinder(
            h = 195,
            r1 = 1.125,
            r2 = 1.125
        );
    translate([motor_pos[0] - 13, motor_pos[1], -200])
        cylinder(
            h = 195,
            r1 = 1.125,
            r2 = 1.125
        );
    translate([motor_pos[0], motor_pos[1] - 13, -200])
        cylinder(
            h = 195,
            r1 = 1.125,
            r2 = 1.125
        );
    translate([trafo_pos[0], trafo_pos[1], -trafo_height])
        cube([
            trafo_size[0],
            trafo_size[1],
            base_height + 1 + trafo_height]);
    translate([trafo_pos[0] + 10, trafo_pos[1] + 10, -200])
        roundedBaseCube([
            trafo_size[0] - 20,
            trafo_size[1] - 20,
            195], 6);
    translate([trafo_pos[0] - 8, trafo_pos[1] + trafo_size[1]/2. - 5., -200])
        roundedBaseCube([4, 10., 295], 2);
    translate([trafo_pos[0] + trafo_size[0] + 4, trafo_pos[1] + trafo_size[1]/2. - 5., -200])
        roundedBaseCube([4, 10., 295], 2);

}

