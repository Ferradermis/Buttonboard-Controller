include <./params.scad>

tol=0.05;

main();

module main(){
    difference(){
        union(){
            hull(){
                cylinder(d=12,h=h_switch_key);
                translate([-w_led_cap/2,-w_led_cap/2,h_led_cap-h_pedestal])
                    rcube([w_led_cap,w_led_cap,h_pedestal],3);
            }
        }
        union(){
            cylinder(d=4,h=25);
        
            cylinder(d=d_switch_key,h=h_switch_key);
            translate([0,0,h_switch_key])
                sphere(d=d_switch_key);
            
            translate([-(w_pedestal+tol)/2,-(w_pedestal+tol)/2,h_led_cap-h_pedestal])
                rcube([w_pedestal+tol,w_pedestal+tol,h_pedestal]);

            translate([-w_wire_notch/2,0,h_led_cap-h_pedestal])
                cube([w_wire_notch,w_pedestal,10]);

            translate([-w_wire_notch/2,w_led_cap/2-2,0])
                cube([w_wire_notch,2,20]);

        }
    }
}