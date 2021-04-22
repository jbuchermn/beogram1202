$fs = 0.1;
$fa = 1;

use <./Round-Anything/polyround.scad>

/*
 * Functions / Reusable modules
 */
function tooth (n, rad, delta, angle) =  
    [for (i=[0 : n + 1]) each [
        [rad * cos(i * angle),
            rad * sin(i * angle)],
        [(rad + delta) * cos((i + .5) * angle),
            (rad + delta) * sin((i + .5) * angle)]
    ]];

function inner_tooth(n, rad, delta, angle) = [for(i = [0 : (2*n)])
    tooth(n, rad, delta, angle)[i]
];

function outer_tooth(n, rad, delta, angle) = [for(i = [1 : (2*n + 1)])
    tooth(n, rad, delta, angle)[i]
];

module roundedBaseCube(dim, r){
    linear_extrude(dim[2]) polygon(polyRound([[0, 0, r], [0, dim[1], r], [dim[0], dim[1], r], [dim[0], 0, r]], fn=20));
}

/*
 * Baseplates
 */
module plate_moveable(){
    difference(){
        union(){
            rotate(-195) polygon([[0, 0], each inner_tooth(10, 48, 2, 3)]);
            translate([-23, 0]) circle(25);
            circle(5);
        }
        translate([-23.5, 0]) circle(19.75);
        circle(2);
    }
}
module plate_moveablecounter(){
    union(){
        rotate(-195) polygon([[53*cos(30), 53*sin(30)], [60, 0], each outer_tooth(9, 48.5, 2, 3)]);
    }
}

/*
 * Moveable part
 */
module mount_moveable(){
    union(){
        linear_extrude(2){
            plate_moveable();
        }
        linear_extrude(15){
            difference(){
                translate([-23.5, 0]) circle(21);
                translate([-23.5, 0]) circle(19.75);
            }
        }
        linear_extrude(7){
            difference(){
                circle(3.5);
                circle(1.75);
            }
        }
    }
}

/*
 * Fixed part
 */
base_height = 3;

button_thickness = 4;
button_height = 20;

module mount_base(){
    triang_corners = [
        [-6 * (1 + cos(71.368))/sin(71.368), 6],
        [104, 6],
        [36., -130]
    ];

    button_corners = [
        [triang_corners[1][0] + sin(36)*75,               triang_corners[1][1] - cos(36)*75],
        [triang_corners[1][0] + sin(36)*75 + sin(-54)*19, triang_corners[1][1] - cos(36)*75 - cos(-54)*19],
        [triang_corners[1][0] + sin(36)*20 + sin(-54)*19, triang_corners[1][1] - cos(36)*20 - cos(-54)*19],
    ];

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
                ], fn=20));
                translate([0, 0]) circle(r = 3);
                translate([100, 0]) circle(r = 3);
                translate([37.7, -111.8]) circle(r = 6);
                translate([37.7, -111.8 - 50]) circle(r = 48);
            }
        }
        translate([button_corners[1][0], button_corners[1][1], 0])
            rotate([0, 0, -54])
                difference(){
                    rotate([0, -90, 0]) roundedBaseCube([button_height, 19, button_thickness], 3);
                    translate([-10, 3, 3]) rotate([90, 0, 90]) roundedBaseCube([3, button_height - 6, 100], 1.5);
                    translate([-10, 3 + 10, 3]) rotate([90, 0, 90]) roundedBaseCube([3, button_height - 6, 100], 1.5);
                }
    }
}

trafo_thickness = 3;
trafo_pos = [20, 0];
trafo_size = [53.5, 44.5];
trafo_height = 6;
trafo_top_height = 1;

module mount_trafo(){
    translate([trafo_pos[0] - trafo_thickness, trafo_pos[1] - trafo_size[1], 0])
        cube([
            trafo_size[0] + 2*trafo_thickness,
            trafo_size[1] + 2*trafo_thickness,
            base_height + trafo_height + trafo_top_height]);
}

module cutout_trafo(){
    translate([trafo_pos[0], trafo_pos[1] - trafo_size[1] + trafo_thickness, -1])
        cube([
            trafo_size[0],
            trafo_size[1],
            base_height + 1 + trafo_height]);

    translate([trafo_pos[0] + 10, trafo_pos[1] - trafo_size[1] + trafo_thickness + 10, -100])
        roundedBaseCube([
            trafo_size[0] - 20,
            trafo_size[1] - 20,
            195], 6);

    translate([trafo_pos[0] - 8, trafo_pos[1] + trafo_thickness - trafo_size[1]/2. - 5., -200])
        roundedBaseCube([4, 10., 295], 2);
    translate([trafo_pos[0] + trafo_size[0] + 4, trafo_pos[1] + trafo_thickness - trafo_size[1]/2. - 5., -200])
        roundedBaseCube([4, 10., 295], 2);
}

motor_pos = [25, -45];
motor_size = [42, 55];

module mount_motor(){
    translate([motor_pos[0] - 7, motor_pos[1] - motor_size[1] - 7, 0])
        roundedBaseCube([motor_size[0] + 14, motor_size[1] + 14, base_height], 33);

    translate([70, -70, 0]) linear_extrude(5 + base_height){
        plate_moveablecounter();
    }
    translate([70, -70, 0]) cylinder(h=base_height, r1=5, r2=5);
}

module cutout_motor(){
    translate([motor_pos[0], motor_pos[1] - motor_size[1], -10])
        roundedBaseCube([motor_size[0], motor_size[1], 300], 23);
    translate([70, -70, -100]) cylinder(h=200, r1=1.5, r2=1.5);
}

module mount_fixed(){
    difference(){
        union(){
            mount_base();
            mount_trafo();
            mount_motor();
        }
        union(){
            cutout_trafo();
            cutout_motor();
        }
    }
}

/*
 * Main
 */

/* plate_moveable(); */
/* plate_moveablecounter(); */

translate([70, -70, base_height])
rotate(6 * 3) 
mount_moveable();
mount_fixed();

