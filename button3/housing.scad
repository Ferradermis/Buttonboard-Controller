include <./params.scad>



main();

_arcade_button_x=13.5;
_arcade_button_y=12;
_arcade_housing_t=2.5;

_housing_x=_arcade_button_x+2*_arcade_housing_t;
_housing_y=_arcade_button_y+2*_arcade_housing_t;
_arcade_button_offset=3;

_housing_h=4+_arcade_button_offset;

module arcade_switch_housing(){
    translate([-_housing_x/2,-_housing_y/2,h_housing-_housing_h])
        cube([_housing_x,_housing_y,_housing_h]);
}

module arcade_switch(){

}

module main(){
    difference(){
        union(){
            iso_thread(m=od_housing,l=h_housing,p=tpitch);
            cylinder(d1=od_rim-2*bevel_rim,d2=od_rim,h=bevel_rim);
            translate([0,0,bevel_rim])
                cylinder(d=od_rim,h=h_rim-bevel_rim);

            
            

        }
        union(){

            cylinder(d=id_housing,h=h_housing-3);

            arcade_switch();

            intersection(){
                cylinder(d=id_housing,h=h_housing);
                translate([-w_wire_notch/2,-od_housing/2,0])
                    cube([w_wire_notch,od_housing,h_housing]);

            }

        }

        difference(){
            union(){
                arcade_switch_housing();
            }
            union(){
                cylinder(d=id_housing,h=h_housing-3);

            arcade_switch();

            intersection(){
                cylinder(d=id_housing,h=h_housing);
                translate([-w_wire_notch/2,-od_housing/2,0])
                    cube([w_wire_notch,od_housing,h_housing]);

            }
            }
        }
    }

    


    //button guides
    difference(){
        union(){
            intersection(){
                cylinder(d=id_housing,h=h_housing);
                translate([-od_housing/2,-w_housing_guide/2,0])
                    cube([od_housing,w_housing_guide,h_housing]);
            }
        }
        union(){
            cylinder(d=id_housing,h=4);
            translate([0,0,4])
                cylinder(d1=id_housing,d2=id_housing-2 * w_housing_guide,h=w_housing_guide);
            cylinder(d=id_housing-2 * w_housing_guide,h=h_housing);
        }
    }

}

