$fn=60;

d_button=24.95;
d_minor=24.6;

t_total=12.56;
l_tab=4;


main();

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
            cube([12.5,12.5,6]);
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
    
}


