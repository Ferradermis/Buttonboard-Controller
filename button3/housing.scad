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
    h_housing=20;
    difference(){
        union(){
            iso_thread(m=od_housing,l=h_housing,p=tpitch);
            cylinder(d1=od_rim-2*bevel_rim,d2=od_rim,h=bevel_rim);
            translate([0,0,bevel_rim])
                cylinder(d=od_rim,h=h_rim-bevel_rim);

            
            
            

        }
        union(){
            cylinder(d1=20,d2=19,h=1);
            translate([0,0,h_housing-1])
            cylinder(d1=19.3,d2=20,h=1);

            translate([0,0,1])
            iso_thread(m=19.5,l=h_housing+2,p=1);
            
            translate([0,0,4])
                cylinder(d=24,h=100);

        }

        
    }

}

