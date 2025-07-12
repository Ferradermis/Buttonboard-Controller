include <./params.scad>


main();

module main(){
    intersection(){
        iso_nut(m=od_housing + thread_tolerance ,p=tpitch,w=6);
        cylinder(d=od_housing + 12,h=6,$fn=11);
    }
}