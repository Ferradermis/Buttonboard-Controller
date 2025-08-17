// Create this as a new file: RobotTelemetryServer.java

package frc.robot;

import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.util.Set;
import java.util.HashSet;
import java.util.function.Supplier;

import edu.wpi.first.wpilibj.Timer;
import edu.wpi.first.wpilibj.DriverStation;

/**
 * UDP Telemetry Server for Robot
 * 
 * Provides simple UDP-based telemetry broadcasting to control boards,
 * driver stations, and other clients. Much simpler than NetworkTables!
 */
public class RobotTelemetryServer {
    
    private final int teamNumber;
    private DatagramSocket udpSocket;
    private DatagramSocket discoverySocket;
    private boolean serverRunning = false;
    private Thread broadcastThread;
    private Thread discoveryThread;
    private Set<String> knownClients = new HashSet<>();
    
    // Telemetry data suppliers (lambda functions to get current values)
    private Supplier<Double> batteryVoltageSupplier = () -> 11.75; // Default hardcoded
    private Supplier<Boolean> shooterReadySupplier = () -> Math.random() > 0.5;
    private Supplier<Integer> shooterSpeedSupplier = () -> (int)(Math.random() * 5000);
    private Supplier<Boolean> intakeDeployedSupplier = () -> Math.random() > 0.7;
    private Supplier<Double> intakePositionSupplier = () -> Math.random();
    private Supplier<String> autoModeSupplier = () -> "Center";
    private Supplier<Integer> autoModeNumberSupplier = () -> 1;
    private Supplier<String> statusMessageSupplier = () -> "Robot Running";
    
    // Network configuration
    private static final int TELEMETRY_PORT = 5574;
    private static final int DISCOVERY_PORT = 5575;
    private static final int BROADCAST_INTERVAL_MS = 100; // 10Hz
    
    public RobotTelemetryServer(int teamNumber) {
        this.teamNumber = teamNumber;
    }
    
    /**
     * Start the telemetry server
     */
    public boolean start() {
        try {
            System.out.println("=== Starting Robot Telemetry Server ===");
            System.out.println("Team: " + teamNumber);
            
            // Create sockets
            udpSocket = new DatagramSocket();
            udpSocket.setBroadcast(true);
            discoverySocket = new DatagramSocket(DISCOVERY_PORT);
            
            serverRunning = true;
            
            System.out.println("✅ Telemetry socket created on port: " + udpSocket.getLocalPort());
            System.out.println("✅ Discovery socket listening on port: " + DISCOVERY_PORT);
            
            // Start threads
            broadcastThread = new Thread(this::broadcastLoop, "TelemetryBroadcast");
            broadcastThread.setDaemon(true);
            broadcastThread.start();
            
            discoveryThread = new Thread(this::discoveryLoop, "TelemetryDiscovery");
            discoveryThread.setDaemon(true);
            discoveryThread.start();
            
            System.out.println("✅ Telemetry server started successfully");
            return true;
            
        } catch (Exception e) {
            System.err.println("❌ Failed to start telemetry server: " + e.getMessage());
            stop();
            return false;
        }
    }
    
    /**
     * Stop the telemetry server
     */
    public void stop() {
        serverRunning = false;
        
        if (broadcastThread != null) {
            broadcastThread.interrupt();
        }
        if (discoveryThread != null) {
            discoveryThread.interrupt();
        }
        if (udpSocket != null) {
            udpSocket.close();
        }
        if (discoverySocket != null) {
            discoverySocket.close();
        }
        
        System.out.println("✅ Telemetry server stopped");
    }
    
    /**
     * Set custom data suppliers for real robot subsystems
     */
    public void setBatteryVoltageSupplier(Supplier<Double> supplier) {
        this.batteryVoltageSupplier = supplier;
    }
    
    public void setShooterSuppliers(Supplier<Boolean> readySupplier, Supplier<Integer> speedSupplier) {
        this.shooterReadySupplier = readySupplier;
        this.shooterSpeedSupplier = speedSupplier;
    }
    
    public void setIntakeSuppliers(Supplier<Boolean> deployedSupplier, Supplier<Double> positionSupplier) {
        this.intakeDeployedSupplier = deployedSupplier;
        this.intakePositionSupplier = positionSupplier;
    }
    
    public void setAutoSuppliers(Supplier<String> modeSupplier, Supplier<Integer> numberSupplier) {
        this.autoModeSupplier = modeSupplier;
        this.autoModeNumberSupplier = numberSupplier;
    }
    
    public void setStatusMessageSupplier(Supplier<String> supplier) {
        this.statusMessageSupplier = supplier;
    }
    
    /**
     * Get server status
     */
    public boolean isRunning() {
        return serverRunning;
    }
    
    public int getClientCount() {
        return knownClients.size();
    }
    
    public Set<String> getKnownClients() {
        return new HashSet<>(knownClients); // Return copy
    }
    
    // Private methods
    
    private void discoveryLoop() {
        System.out.println("🔍 Discovery listener started");
        byte[] buffer = new byte[512];
        
        while (serverRunning) {
            try {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                discoverySocket.receive(packet);
                
                String message = new String(packet.getData(), 0, packet.getLength());
                String clientIP = packet.getAddress().getHostAddress();
                
                System.out.println("📡 Discovery from " + clientIP + ": " + message);
                
                // Simple parsing - look for teensy control board
                if (message.contains("teensy_control_board") || message.contains("control_board")) {
                    if (knownClients.add(clientIP)) {
                        System.out.println("✅ Registered new client: " + clientIP);
                        System.out.println("📊 Total clients: " + knownClients.size());
                    }
                }
                
            } catch (Exception e) {
                if (serverRunning) {
                    System.err.println("Discovery error: " + e.getMessage());
                }
            }
        }
        
        System.out.println("🛑 Discovery listener stopped");
    }
    
    private void broadcastLoop() {
        System.out.println("🚀 Telemetry broadcast started");
        
        while (serverRunning) {
            try {
                String telemetryJson = createTelemetryPacket();
                byte[] data = telemetryJson.getBytes();
                
                // Send to all known clients
                for (String clientIP : knownClients) {
                    try {
                        InetAddress clientAddr = InetAddress.getByName(clientIP);
                        DatagramPacket packet = new DatagramPacket(data, data.length, clientAddr, TELEMETRY_PORT);
                        udpSocket.send(packet);
                    } catch (Exception e) {
                        System.err.println("Failed to send to " + clientIP + ": " + e.getMessage());
                    }
                }
                
                // Also broadcast to subnet for discovery
                try {
                    String broadcastIP = calculateBroadcastIP();
                    InetAddress broadcastAddr = InetAddress.getByName(broadcastIP);
                    DatagramPacket packet = new DatagramPacket(data, data.length, broadcastAddr, TELEMETRY_PORT);
                    udpSocket.send(packet);
                } catch (Exception e) {
                    System.err.println("Broadcast failed: " + e.getMessage());
                }
                
                Thread.sleep(BROADCAST_INTERVAL_MS);
                
            } catch (InterruptedException e) {
                break;
            } catch (Exception e) {
                System.err.println("Broadcast error: " + e.getMessage());
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException ie) {
                    break;
                }
            }
        }
        
        System.out.println("🛑 Telemetry broadcast stopped");
    }
    
    private String createTelemetryPacket() {
        StringBuilder json = new StringBuilder();
        json.append("{");
        
        // Metadata
        json.append("\"timestamp\":").append(Timer.getFPGATimestamp()).append(",");
        json.append("\"team\":").append(teamNumber).append(",");
        
        // Battery
        json.append("\"battery\":{");
        double batteryVoltage = batteryVoltageSupplier.get();
        json.append("\"voltage\":").append(batteryVoltage).append(",");
        json.append("\"isLow\":").append(batteryVoltage < 11.5);
        json.append("},");
        
        // Robot state
        json.append("\"robot\":{");
        json.append("\"enabled\":").append(DriverStation.isEnabled()).append(",");
        json.append("\"autonomous\":").append(DriverStation.isAutonomous()).append(",");
        json.append("\"teleop\":").append(DriverStation.isTeleop()).append(",");
        json.append("\"mode\":\"").append(getRobotModeString()).append("\"");
        json.append("},");
        
        // Alliance
        json.append("\"alliance\":{");
        String allianceColor = "unknown";
        var alliance = DriverStation.getAlliance();
        if (alliance.isPresent()) {
            allianceColor = alliance.get().toString().toLowerCase();
        }
        json.append("\"color\":\"").append(allianceColor).append("\",");
        json.append("\"isRed\":").append(allianceColor.equals("red"));
        json.append("},");
        
        // Match
        json.append("\"match\":{");
        json.append("\"timeRemaining\":").append(DriverStation.getMatchTime());
        json.append("},");
        
        // Subsystems (using suppliers)
        json.append("\"shooter\":{");
        json.append("\"ready\":").append(shooterReadySupplier.get()).append(",");
        json.append("\"speed\":").append(shooterSpeedSupplier.get());
        json.append("},");
        
        json.append("\"intake\":{");
        json.append("\"deployed\":").append(intakeDeployedSupplier.get()).append(",");
        json.append("\"position\":").append(intakePositionSupplier.get());
        json.append("},");
        
        // Auto
        json.append("\"auto\":{");
        json.append("\"selectedMode\":\"").append(autoModeSupplier.get()).append("\",");
        json.append("\"modeNumber\":").append(autoModeNumberSupplier.get());
        json.append("},");
        
        // Status
        json.append("\"status\":{");
        json.append("\"message\":\"").append(statusMessageSupplier.get()).append("\",");
        json.append("\"heartbeat\":").append((int)(Timer.getFPGATimestamp() * 10) % 1000);
        json.append("}");
        
        json.append("}");
        return json.toString();
    }
    
    private String getRobotModeString() {
        if (DriverStation.isDisabled()) return "Disabled";
        if (DriverStation.isAutonomous()) return "Autonomous";
        if (DriverStation.isTeleop()) return "Teleop";
        if (DriverStation.isTest()) return "Test";
        return "Unknown";
    }
    
    private String calculateBroadcastIP() {
        // Calculate broadcast IP from team number: 10.TE.AM.255
        int firstOctet = teamNumber / 100;
        int secondOctet = teamNumber % 100;
        return "10." + firstOctet + "." + secondOctet + ".255";
    }
}