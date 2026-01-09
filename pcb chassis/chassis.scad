$fn=60;
inch=25.45;

t_plate=3;
w_plate=120;
h_plate=120;
r_plate=6;



pcb_holes=[3.1*inch,3.45*inch];


main();


/*LCD 1602 Modules*/
w_lcd=72.5;
h_lcd=24.4;
lcd_holes=[75.4,31.4];
lcd_holes_y_offset=0.75;
module lcd1602Mount(){
    
    translate([w_plate/2-lcd_holes[0]/2,h_plate/2-lcd_holes[1]/2 + lcd_holes_y_offset,0])
        for(p=[[0,0,0],[0,lcd_holes[1],0],[lcd_holes[0],lcd_holes[1],0],[lcd_holes[0],0,0]])
            translate(p)
                cylinder(d=6,h=7);
}
module lcd1602MountNeg(){
    translate([w_plate/2-lcd_holes[0]/2,h_plate/2-lcd_holes[1]/2 + lcd_holes_y_offset,1])
        for(p=[[0,0,0],[0,lcd_holes[1],0],[lcd_holes[0],lcd_holes[1],0],[lcd_holes[0],0,0]])
            translate(p)
                cylinder(d=3,h=10);
}
/*END LCD 1602 Modules*/

module main(){
    difference(){
        union(){
            roundcube([w_plate,h_plate,t_plate],r_plate);

            lcd1602Mount();

            translate([w_plate/2-pcb_holes[0]/2,h_plate/2-pcb_holes[1]/2 ,0])
                for(p=[[0,0,0],[0,pcb_holes[1],0],[pcb_holes[0],pcb_holes[1],0],[pcb_holes[0],0,0]])
                    translate(p)
                        cylinder(d1=8,d2=6,h=24);

        }
        union(){
            translate([w_plate/2-w_lcd/2,h_plate/2-h_lcd/2,0]){
                cube([w_lcd,h_lcd,10]);
            }

            lcd1602MountNeg();

            translate([w_plate/2-pcb_holes[0]/2,h_plate/2-pcb_holes[1]/2 ,1])
                for(p=[[0,0,0],[0,pcb_holes[1],0],[pcb_holes[0],pcb_holes[1],0],[pcb_holes[0],0,0]])
                    translate(p)
                        cylinder(d=3,h=24);


            for(p=[
                [r_plate,r_plate,0],
                [r_plate,h_plate-r_plate,0],
                [w_plate-r_plate,h_plate-r_plate,0],
                [w_plate-r_plate,r_plate,0]
            ])
            translate(p)cylinder(d=4,h=10);
            
        }
    }
}



module roundcube(d,r=4){
    hull(){
        for(p=[[r,r,0],[d[0]-r,r,0],[d[0]-r,d[1]-r,0],[r,d[1]-r,0]])
            translate(p)
            cylinder(r=r,h=d[2]);
    }
}