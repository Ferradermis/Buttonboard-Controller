include <./params.scad>


main();

module main(){
    intersection(){
        iso_nut(m=od_housing + thread_tolerance ,p=tpitch,w=h_nut+h_nut_rim);
        
        union(){
            cylinder(d=od_housing + 6,h=h_nut,$fn=16);
            cylinder(d=32,h=h_nut+h_nut_rim);
        }
    }
}