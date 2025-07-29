#include "CuttingParameters.h"

/**
 * Cutting Parameters Demo
 * 
 * This file demonstrates how to use the cutting parameters system
 * to calculate RPM recommendations for different scenarios.
 * 
 * Usage examples for future HMI integration.
 */

void demonstrateCuttingParameters() {
    Serial.println("=== Cutting Parameters Demo ===");
    
    // Example 1: Aluminum with coated carbide, rough turning, 25mm diameter
    RPMResult result1 = cuttingParams.calculateRPM(
        MATERIAL_ALUMINUM, 
        TOOL_CARBIDE_COATED, 
        OP_ROUGH_TURNING, 
        25.0f,  // 25mm diameter
        true    // Metric units
    );
    
    Serial.println("Example 1: Aluminum + Coated Carbide + Rough Turn (25mm)");
    Serial.println(result1.recommendation);
    Serial.println("RPM: " + String(result1.rpm) + ", Speed: " + String(result1.cuttingSpeed, 0) + " m/min");
    Serial.println();
    
    // Example 2: Mild steel with HSS, finish turning, 1 inch diameter
    RPMResult result2 = cuttingParams.calculateRPM(
        MATERIAL_MILD_STEEL, 
        TOOL_HSS, 
        OP_FINISH_TURNING, 
        1.0f,   // 1 inch diameter
        false   // Imperial units
    );
    
    Serial.println("Example 2: Mild Steel + HSS + Finish Turn (1\")");
    Serial.println(result2.recommendation);
    Serial.println("RPM: " + String(result2.rpm) + ", Speed: " + String(result2.cuttingSpeed, 0) + " SFM");
    Serial.println();
    
    // Example 3: Stainless steel with CBN, threading, 12mm diameter
    RPMResult result3 = cuttingParams.calculateRPM(
        MATERIAL_STAINLESS_300, 
        TOOL_CBN, 
        OP_THREADING, 
        12.0f,  // 12mm diameter
        true    // Metric units
    );
    
    Serial.println("Example 3: Stainless 300 + CBN + Threading (12mm)");
    Serial.println(result3.recommendation);
    Serial.println("RPM: " + String(result3.rpm) + ", Speed: " + String(result3.cuttingSpeed, 0) + " m/min");
    Serial.println();
    
    // Example 4: Titanium with uncoated carbide, facing, 0.5 inch diameter
    RPMResult result4 = cuttingParams.calculateRPM(
        MATERIAL_TITANIUM, 
        TOOL_CARBIDE_UNCOATED, 
        OP_FACING, 
        0.5f,   // 0.5 inch diameter
        false   // Imperial units
    );
    
    Serial.println("Example 4: Titanium + Carbide + Facing (0.5\")");
    Serial.println(result4.recommendation);
    Serial.println("RPM: " + String(result4.rpm) + ", Speed: " + String(result4.cuttingSpeed, 0) + " SFM");
    Serial.println();
    
    // Example 5: Material selection by name
    MaterialCategory material = cuttingParams.getMaterialByName("A36 steel");
    ToolType tool = cuttingParams.getToolByName("coated carbide");
    
    RPMResult result5 = cuttingParams.calculateRPM(
        material, 
        tool, 
        OP_ROUGH_TURNING, 
        50.0f,  // 50mm diameter
        true    // Metric units
    );
    
    Serial.println("Example 5: A36 Steel + Coated Carbide + Rough Turn (50mm)");
    Serial.println(result5.recommendation);
    Serial.println("RPM: " + String(result5.rpm) + ", Speed: " + String(result5.cuttingSpeed, 0) + " m/min");
    Serial.println();
    
    // Example 6: Material selection by tensile strength
    MaterialCategory materialByStrength = cuttingParams.getMaterialByTensileStrength(750.0f); // 750 MPa
    
    RPMResult result6 = cuttingParams.calculateRPM(
        materialByStrength, 
        TOOL_CARBIDE_COATED, 
        OP_FINISH_TURNING, 
        30.0f,  // 30mm diameter
        true    // Metric units
    );
    
    Serial.println("Example 6: Material (750 MPa) + Coated Carbide + Finish Turn (30mm)");
    Serial.println(result6.recommendation);
    Serial.println("RPM: " + String(result6.rpm) + ", Speed: " + String(result6.cuttingSpeed, 0) + " m/min");
    Serial.println();
}

void demonstrateValidation() {
    Serial.println("=== Validation Examples ===");
    
    // Test invalid diameter
    RPMResult invalidResult = cuttingParams.calculateRPM(
        MATERIAL_ALUMINUM, 
        TOOL_CARBIDE_COATED, 
        OP_ROUGH_TURNING, 
        0.1f,   // Too small
        true
    );
    
    Serial.println("Invalid diameter (0.1mm): " + invalidResult.recommendation);
    Serial.println("Valid: " + String(invalidResult.isValid ? "Yes" : "No"));
    Serial.println();
    
    // Test valid ranges
    Serial.println("Valid diameter range: " + String(cuttingParams.getMinDiameter()) + 
                   " to " + String(cuttingParams.getMaxDiameter()) + " mm");
    Serial.println();
}

void demonstrateMaterialInfo() {
    Serial.println("=== Available Materials ===");
    String* materialNames = cuttingParams.getMaterialNames();
    for (int i = 0; i < MATERIAL_COUNT; i++) {
        Serial.println(String(i) + ": " + materialNames[i]);
    }
    Serial.println();
    
    Serial.println("=== Available Tools ===");
    String* toolNames = cuttingParams.getToolNames();
    for (int i = 0; i < TOOL_COUNT; i++) {
        Serial.println(String(i) + ": " + toolNames[i]);
    }
    Serial.println();
    
    Serial.println("=== Available Operations ===");
    String* operationNames = cuttingParams.getOperationNames();
    for (int i = 0; i < OP_COUNT; i++) {
        Serial.println(String(i) + ": " + operationNames[i]);
    }
    Serial.println();
}

// Function to be called from main setup() for testing
void runCuttingParametersDemo() {
    Serial.println("Starting Cutting Parameters Demo...");
    Serial.println();
    
    demonstrateMaterialInfo();
    demonstrateCuttingParameters();
    demonstrateValidation();
    
    Serial.println("=== Demo Complete ===");
    Serial.println();
    Serial.println("This system provides intelligent RPM recommendations based on:");
    Serial.println("- Tool type (HSS, Carbide, CBN, Diamond, Ceramic)");
    Serial.println("- Material properties (tensile strength, type)");
    Serial.println("- Operation type (rough, finish, facing, threading, parting)");
    Serial.println("- Workpiece diameter");
    Serial.println("- Measurement units (metric/imperial)");
    Serial.println();
    Serial.println("Ready for HMI integration with Nextion touch screen!");
} 