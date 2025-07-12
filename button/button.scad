$fn=60;

d_button=24.95;
d_minor=24.6;

t_total=12.56;
l_tab=4;

t_bevel=1.5;
    t_face=t_bevel+1.5;
    h_face=4;
    t_shell=1.5;


main();

//cap();

//clip_shape();

module cap(){
    cap_size=16;
    cap_base_d=13;
    cap_height=9.5;
    cap_stem_height=4;
    cap_cavity_size=13.5;
    difference(){
        union(){
            hull(){
                cylinder(d=cap_base_d,h=1);
                translate([-cap_size/2,-cap_size/2,cap_stem_height])
                    rcube([16,16,cap_height-cap_stem_height]);
            }
        }
        union(){
            translate([-cap_cavity_size/2,-cap_cavity_size/2,cap_stem_height])
                    rcube([cap_cavity_size,cap_cavity_size,15]);

            //button slot
            translate([-1.1,-3.3,0])
                cube([2.2,6.6,14]);
            //notch for wires
            translate([0,-3.5,cap_stem_height])
                cube([10,7,10]);
            
        }
    }
}


module clip_shape(){
    translate([d_minor/2-1.6,.5+t_bevel])
    polygon(points=[
        [0,0],
        [1.6,0],
        [1.6,8.5],
        [2.81,8.5],
        [1.6,11],
        [0,11]

    ]);
}

module main(){
    


    difference(){
        union(){
            cylinder(d1=d_button-t_bevel*2,d2=d_button,h=t_bevel);
            translate([0,0,t_bevel])
                cylinder(d=d_button,h=h_face);
        }
        union(){
            translate([0,0,t_face])
                cylinder(d=d_button-t_shell*2,h=h_face);
        }
    }

    difference(){
        union(){
            translate([0,0,t_bevel])
                cylinder(d=d_minor,h=11);
        }
        union(){
            translate([-20,-4,4.5])
                cube([40,8,20]);

            translate([-5,-20,4.5])
                cube([10,40,20]);

            cylinder(d=d_button - 2*t_shell, h=20);
        }
    }

    pixel_holder_size=12.5;
    
    difference(){
        union(){
            translate([-pixel_holder_size/2,-pixel_holder_size/2,0])
                rcube([pixel_holder_size,pixel_holder_size,t_face+4]);
        }
        union(){
            translate([0,-3.5,4])
                cube([10,7,10]);

            translate([-5.25,-3,2.5])
                cube([10.5,6,10]);
            translate([-3,-5.25,2.5])
                cube([6,10.5,10]);
            
            translate([-5,-5,3.7])
                cube([10,10,10]);
                
        }
    }

    intersection(){
        translate([-20,-2.5,0])
            cube([40,5,40]);
        rotate_extrude(){
            clip_shape();
        }
    }
    
}

module rcube(size,r=1){
    hull(){
        for(x=[r,size[0]-r])
            for (y=[r,size[1]-r])
                translate([x,y,0])
                    cylinder(r=r,h=size[2]);
    }
            
}


