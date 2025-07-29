#ifndef CUTTING_PARAMETERS_H
#define CUTTING_PARAMETERS_H

#include <Arduino.h>

/**
 * Cutting Parameters Backend System
 * 
 * Provides intelligent cutting speed recommendations based on:
 * - Tool type (HSS, Carbide, CBN, etc.)
 * - Material type and properties
 * - Workpiece diameter
 * - Operation type (turning, facing, threading)
 * 
 * This is a standalone module that doesn't influence other system components.
 * Designed for future HMI integration with Nextion touch screen.
 */

// Tool Type Definitions
enum ToolType {
    TOOL_HSS = 0,           // High Speed Steel
    TOOL_CARBIDE_UNCOATED,  // Uncoated Carbide
    TOOL_CARBIDE_COATED,    // Coated Carbide (TiN, TiCN, Al2O3, etc.)
    TOOL_CBN,               // Cubic Boron Nitride
    TOOL_DIAMOND,           // Polycrystalline Diamond
    TOOL_CERAMIC,           // Ceramic inserts
    TOOL_COUNT              // Total number of tool types
};

// Material Categories (by tensile strength and type)
enum MaterialCategory {
    MATERIAL_ALUMINUM = 0,      // Aluminum alloys (soft)
    MATERIAL_BRASS_BRONZE,      // Brass, Bronze, Copper
    MATERIAL_MILD_STEEL,        // Low carbon steel (A36, 1018, etc.)
    MATERIAL_MEDIUM_STEEL,      // Medium carbon steel (1045, 4140, etc.)
    MATERIAL_HARD_STEEL,        // High carbon steel, tool steel
    MATERIAL_STAINLESS_300,     // Austenitic stainless (304, 316, etc.)
    MATERIAL_STAINLESS_400,     // Martensitic stainless (410, 420, etc.)
    MATERIAL_TITANIUM,          // Titanium alloys
    MATERIAL_INCONEL,           // Inconel, Hastelloy, superalloys
    MATERIAL_CAST_IRON,         // Gray, ductile, malleable iron
    MATERIAL_PLASTIC,           // Plastics, composites
    MATERIAL_WOOD,              // Wood, MDF, etc.
    MATERIAL_COUNT              // Total number of material categories
};

// Operation Types
enum OperationType {
    OP_ROUGH_TURNING = 0,   // Rough turning
    OP_FINISH_TURNING,      // Finish turning
    OP_FACING,              // Facing operations
    OP_THREADING,           // Threading operations
    OP_PARTING,             // Parting/grooving
    OP_COUNT                // Total number of operation types
};

// Cutting Speed Factors (Surface Feet per Minute for imperial, m/min for metric)
struct CuttingSpeedData {
    float baseSpeed;        // Base cutting speed for the material/tool combination
    float diameterFactor;   // Factor to apply based on diameter
    float operationFactor;  // Factor to apply based on operation type
    float toolFactor;       // Factor to apply based on tool type
};

// RPM Calculation Result
struct RPMResult {
    int rpm;                // Calculated RPM
    float cuttingSpeed;     // Actual cutting speed achieved
    String recommendation;  // Text recommendation
    bool isValid;           // Whether the calculation is valid
};

class CuttingParameters {
private:
    // Cutting speed database (SFM for imperial, m/min for metric)
    // [Material][ToolType] = base cutting speed
    static const float CUTTING_SPEEDS[MATERIAL_COUNT][TOOL_COUNT];
    
    // Diameter factors (multipliers for different diameter ranges)
    static const float DIAMETER_FACTORS[5];  // 5 diameter ranges
    
    // Operation type factors
    static const float OPERATION_FACTORS[OP_COUNT];
    
    // Tool type factors (relative to HSS)
    static const float TOOL_FACTORS[TOOL_COUNT];
    
    // Material tensile strength ranges (MPa)
    static const float MATERIAL_TENSILE_STRENGTHS[MATERIAL_COUNT][2];  // [min, max]
    
    // Helper functions
    float getDiameterFactor(float diameter) const;
    int calculateRPM(float cuttingSpeed, float diameter, bool isMetric) const;
    float getBaseCuttingSpeed(MaterialCategory material, ToolType tool) const;
    
public:
    CuttingParameters();
    
    // Main RPM calculation function
    RPMResult calculateRPM(MaterialCategory material, ToolType tool, 
                          OperationType operation, float diameter, bool isMetric) const;
    
    // Material selection helpers
    MaterialCategory getMaterialByTensileStrength(float tensileStrengthMPa) const;
    MaterialCategory getMaterialByName(const String& materialName) const;
    
    // Tool selection helpers
    ToolType getToolByName(const String& toolName) const;
    
    // Information functions
    String getMaterialName(MaterialCategory material) const;
    String getToolName(ToolType tool) const;
    String getOperationName(OperationType operation) const;
    
    // Validation functions
    bool isValidDiameter(float diameter) const;
    bool isValidMaterial(MaterialCategory material) const;
    bool isValidTool(ToolType tool) const;
    bool isValidOperation(OperationType operation) const;
    
    // Range information
    float getMinDiameter() const { return 0.5f; }  // 0.5mm minimum
    float getMaxDiameter() const { return 500.0f; } // 500mm maximum
    
    // Unit conversion helpers
    float mpmToSFM(float mpm) const { return mpm * 3.28084f; }
    float sfmToMPM(float sfm) const { return sfm / 3.28084f; }
    
    // Get all available options for UI
    String* getMaterialNames() const;
    String* getToolNames() const;
    String* getOperationNames() const;
};

// Global instance
extern CuttingParameters cuttingParams;

#endif // CUTTING_PARAMETERS_H 