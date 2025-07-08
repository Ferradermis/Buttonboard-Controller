$fn=60;

d_button=24.95;
d_minor=24.6;

t_total=12.56;
l_tab=4;


//main();

cap();

//clip_shape();

module cap(){
    difference(){
        union(){
            hull(){
                cylinder(d=13,h=1);
                translate([-8,-8,4])
                    rcube([16,16,5.5]);
            }
        }
        union(){
            translate([-6.75,-6.75,4])
                    rcube([13.5,13.5,15]);

            translate([-1.1,-3.3,0])
                cube([2.2,6.6,14]);

            translate([0,-3.5,4])
                cube([10,7,10]);
            
        }
    }
}


module clip_shape(){
    translate([d_minor/2-1.6,1])
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
            cylinder(d1=d_button-1,d2=d_button,h=.5);
            translate([0,0,.5])
                cylinder(d=d_button,h=4);
        }
        union(){
            translate([0,0,2])
                cylinder(d=d_button-3,h=4);
        }
    }

    difference(){
        union(){
            translate([0,0,.5])
                cylinder(d=d_minor,h=11);
        }
        union(){
            translate([-20,-4,4.5])
                cube([40,8,20]);

            translate([-5,-20,4.5])
                cube([10,40,20]);

            cylinder(d=d_button - 3, h=20);
        }
    }

    
    difference(){
        union(){
            translate([-6.25,-6.25,0])
                rcube([12.5,12.5,6]);
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


