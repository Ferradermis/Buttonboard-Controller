include <./params.scad>


main();

module main(){
    intersection(){
        iso_nut(m=od_housing + thread_tolerance ,p=tpitch,w=h_nut+3);
        
        union(){
            cylinder(d=od_housing + 10,h=h_nut,$fn=nut_sides);
            cylinder(d=32,h=h_nut+3);
        }
    }
}