// Clean integration in your Robot.java

package frc.robot;

import edu.wpi.first.wpilibj.TimedRobot;
import edu.wpi.first.wpilibj.Timer;
import edu.wpi.first.wpilibj.PowerDistribution;
import edu.wpi.first.wpilibj.RobotController;
import edu.wpi.first.networktables.NetworkTable;
import edu.wpi.first.networktables.NetworkTableInstance;

public class Robot extends TimedRobot {
  private int callCount = 0;
    // Your existing robot components
    private PowerDistribution pdp;
    private NetworkTable controlBoardTable;
    private Timer publishTimer;
    
    // Simple telemetry server
    private RobotTelemetryServer telemetryServer;
    
    @Override
    public void robotInit() {
        System.out.println("=== Robot Init ===");
        
        // Your existing initialization
        pdp = new PowerDistribution();
        controlBoardTable = NetworkTableInstance.getDefault().getTable("ControlBoard");
        publishTimer = new Timer();
        publishTimer.start();
        
        // Start telemetry server
        telemetryServer = new RobotTelemetryServer(6574); // Your team number
        
        // Configure real data suppliers (replace with your actual subsystems)
        telemetryServer.setBatteryVoltageSupplier(() -> {
            // Replace with actual battery reading
            return RobotController.getBatteryVoltage(); // or pdp.getVoltage() if you have a PDP
        });
        
        // Example: Connect to real shooter subsystem
        // telemetryServer.setShooterSuppliers(
        //     () -> shooter.isReady(),           // Real shooter ready status
        //     () -> (int) shooter.getSpeed()     // Real shooter speed
        // );
        
        // Example: Connect to real intake subsystem  
        // telemetryServer.setIntakeSuppliers(
        //     () -> intake.isDeployed(),         // Real intake deployed status
        //     () -> intake.getPosition()         // Real intake position
        // );
        
        // Start the server
        if (telemetryServer.start()) {
            System.out.println("✅ Telemetry server started successfully");
        } else {
            System.out.println("❌ Telemetry server failed to start");
        }
        
        System.out.println("=== Robot Init Complete ===");
    }
    
    @Override
    public void robotPeriodic() {
        // Your existing periodic code
        if (publishTimer.hasElapsed(0.1)) {
            // Your existing NetworkTables publishing if you want to keep it
            controlBoardTable.getEntry("battery/voltage").setDouble(11.75);
            publishTimer.restart();
        }
        
        // Optional: Print telemetry server status occasionally
        
        callCount++;
        if (callCount % 50 == 0) { // Every 5 seconds
            if (telemetryServer.isRunning()) {
                System.out.println("📡 Telemetry server: " + telemetryServer.getClientCount() + " clients connected");
            }
        }
    }
    
    @Override
    public void disabledInit() {
        System.out.println("Robot disabled - telemetry continues running");
    }
    
    @Override
    public void autonomousInit() {
        System.out.println("Autonomous started");
    }
    
    @Override
    public void teleopInit() {
        System.out.println("Teleop started");
    }
    
    @Override
    public void testInit() {
        System.out.println("Test mode started");
    }
    
    // Optional: Clean shutdown when robot code stops
    @Override
    public void close() {
        if (telemetryServer != null) {
            telemetryServer.stop();
        }
        super.close();
    }
}