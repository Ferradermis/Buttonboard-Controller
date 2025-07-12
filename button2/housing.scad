include <./params.scad>



main();

module main(){
    difference(){
        union(){
            iso_thread(m=od_housing,l=25,p=tpitch);
            cylinder(d1=od_rim-2*bevel_rim,d2=od_rim,h=bevel_rim);
            translate([0,0,bevel_rim])
                cylinder(d=od_rim,h=h_rim-bevel_rim);
        }
        union(){
            cylinder(d=id_housing,h=50);
        }
    }
}

