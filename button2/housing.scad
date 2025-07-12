include <./params.scad>



main();

module main(){
    difference(){
        union(){
            iso_thread(m=od_housing,l=h_housing,p=tpitch);
            cylinder(d1=od_rim-2*bevel_rim,d2=od_rim,h=bevel_rim);
            translate([0,0,bevel_rim])
                cylinder(d=od_rim,h=h_rim-bevel_rim);

            //fins
            intersection(){
                union(){
                    for (zr=[0,90])
                        rotate([0,0,zr])
                            translate([-od_rim/2,-w_fin/2,h_rim])
                                cube([od_rim,w_fin,h_fin]);
                }
                translate([0,0,h_rim])
                    cylinder(d1=od_housing+1,d2=id_housing,h=h_fin);
            }

        }
        union(){
            cylinder(d=id_housing,h=h_button_travel);

            translate([0,0,h_button_travel])
                cylinder(d1=id_housing,d2=d_switch_shoulder,h=(id_housing-d_switch_shoulder)/2);

            cylinder(d=d_switch_body,h=h_housing);

            intersection(){
                cylinder(d=id_housing,h=h_housing);
                translate([-w_wire_notch/2,0,0])
                    cube([w_wire_notch,od_housing/2,h_housing]);

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

