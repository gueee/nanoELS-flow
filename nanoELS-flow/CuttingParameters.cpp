#include "CuttingParameters.h"

// Global instance
CuttingParameters cuttingParams;

// Cutting speed database (SFM for imperial, m/min for metric)
// [Material][ToolType] = base cutting speed
// Data based on industry standards from Sandvik, Seco, Kennametal, and other manufacturers
const float CuttingParameters::CUTTING_SPEEDS[MATERIAL_COUNT][TOOL_COUNT] = {
    //       HSS    Carbide  Carbide  CBN     Diamond  Ceramic
    //              (uncoated) (coated)
    {200,   800,    1000,   1500,   2000,    1200},      // Aluminum alloys
    {150,   600,    800,    1200,   1500,    900},       // Brass, Bronze, Copper
    {100,   400,    600,    800,    1000,    600},       // Low carbon steel
    {80,    300,    450,    600,    800,    450},        // Medium carbon steel
    {60,    200,    300,    400,    600,    300},        // High carbon steel
    {70,    250,    400,    500,    700,    400},        // Austenitic stainless
    {60,    200,    300,    400,    600,    300},        // Martensitic stainless
    {40,    150,    200,    300,    400,    200},        // Titanium alloys
    {30,    100,    150,    200,    300,    150},        // Inconel, superalloys
    {80,    300,    450,    600,    800,    450},        // Cast iron
    {300,   1200,   1500,   2000,   2500,   1500},       // Plastics
    {500,   2000,   2500,   3000,   4000,   2500}        // Wood
};

// Diameter factors (multipliers for different diameter ranges)
// Smaller diameters need higher RPM, larger diameters need lower RPM
const float CuttingParameters::DIAMETER_FACTORS[5] = {
    1.5f,   // 0.5-5mm (0.02-0.2"): Increase speed for small diameters
    1.2f,   // 5-20mm (0.2-0.8"): Slight increase
    1.0f,   // 20-50mm (0.8-2"): Standard speed
    0.8f,   // 50-100mm (2-4"): Slight decrease
    0.6f    // 100-500mm (4-20"): Significant decrease for large diameters
};

// Operation type factors
// Different operations require different cutting speeds
const float CuttingParameters::OPERATION_FACTORS[OP_COUNT] = {
    1.0f,   // OP_ROUGH_TURNING: Standard speed
    0.8f,   // OP_FINISH_TURNING: Slightly slower for better finish
    0.9f,   // OP_FACING: Slightly slower due to interrupted cut
    0.6f,   // OP_THREADING: Much slower for precision
    0.7f    // OP_PARTING: Slower due to high tool pressure
};

// Tool type factors (relative to HSS)
// Modern tools can run faster than HSS
const float CuttingParameters::TOOL_FACTORS[TOOL_COUNT] = {
    1.0f,   // TOOL_HSS: Baseline
    2.0f,   // TOOL_CARBIDE_UNCOATED: 2x faster than HSS
    2.5f,   // TOOL_CARBIDE_COATED: 2.5x faster than HSS
    3.0f,   // TOOL_CBN: 3x faster than HSS
    4.0f,   // TOOL_DIAMOND: 4x faster than HSS
    2.0f    // TOOL_CERAMIC: 2x faster than HSS
};

// Material tensile strength ranges (MPa)
// Used for material selection based on known properties
const float CuttingParameters::MATERIAL_TENSILE_STRENGTHS[MATERIAL_COUNT][2] = {
    {200,  400},   // MATERIAL_ALUMINUM: 200-400 MPa
    {200,  600},   // MATERIAL_BRASS_BRONZE: 200-600 MPa
    {400,  600},   // MATERIAL_MILD_STEEL: 400-600 MPa
    {600,  900},   // MATERIAL_MEDIUM_STEEL: 600-900 MPa
    {900,  1500},  // MATERIAL_HARD_STEEL: 900-1500 MPa
    {500,  800},   // MATERIAL_STAINLESS_300: 500-800 MPa
    {800,  1200},  // MATERIAL_STAINLESS_400: 800-1200 MPa
    {800,  1200},  // MATERIAL_TITANIUM: 800-1200 MPa
    {800,  1400},  // MATERIAL_INCONEL: 800-1400 MPa
    {200,  400},   // MATERIAL_CAST_IRON: 200-400 MPa
    {30,   200},   // MATERIAL_PLASTIC: 30-200 MPa
    {20,   100}    // MATERIAL_WOOD: 20-100 MPa
};

CuttingParameters::CuttingParameters() {
    // Constructor - no initialization needed for static data
}

float CuttingParameters::getDiameterFactor(float diameter) const {
    if (diameter < 5.0f) return DIAMETER_FACTORS[0];
    if (diameter < 20.0f) return DIAMETER_FACTORS[1];
    if (diameter < 50.0f) return DIAMETER_FACTORS[2];
    if (diameter < 100.0f) return DIAMETER_FACTORS[3];
    return DIAMETER_FACTORS[4];
}

float CuttingParameters::getBaseCuttingSpeed(MaterialCategory material, ToolType tool) const {
    if (material >= MATERIAL_COUNT || tool >= TOOL_COUNT) return 100.0f; // Default fallback
    return CUTTING_SPEEDS[material][tool];
}

int CuttingParameters::calculateRPM(float cuttingSpeed, float diameter, bool isMetric) const {
    if (diameter <= 0) return 0;
    
    // Formula: RPM = (Cutting Speed * 1000) / (π * diameter)
    // For imperial: RPM = (SFM * 12) / (π * diameter_inches)
    // For metric: RPM = (m/min * 1000) / (π * diameter_mm)
    
    float rpm;
    if (isMetric) {
        rpm = (cuttingSpeed * 1000.0f) / (3.14159f * diameter);
    } else {
        rpm = (cuttingSpeed * 12.0f) / (3.14159f * diameter);
    }
    
    return (int)rpm;
}

RPMResult CuttingParameters::calculateRPM(MaterialCategory material, ToolType tool, 
                                        OperationType operation, float diameter, bool isMetric) const {
    RPMResult result;
    result.isValid = false;
    result.rpm = 0;
    result.cuttingSpeed = 0.0f;
    result.recommendation = "";
    
    // Validate inputs
    if (!isValidMaterial(material) || !isValidTool(tool) || 
        !isValidOperation(operation) || !isValidDiameter(diameter)) {
        result.recommendation = "Invalid parameters";
        return result;
    }
    
    // Get base cutting speed
    float baseSpeed = getBaseCuttingSpeed(material, tool);
    
    // Apply factors
    float diameterFactor = getDiameterFactor(diameter);
    float operationFactor = OPERATION_FACTORS[operation];
    float toolFactor = TOOL_FACTORS[tool];
    
    // Calculate final cutting speed
    float finalSpeed = baseSpeed * diameterFactor * operationFactor * toolFactor;
    
    // Calculate RPM
    int rpm = calculateRPM(finalSpeed, diameter, isMetric);
    
    // Validate RPM range (50-3000 RPM typical for lathes)
    if (rpm < 50) rpm = 50;
    if (rpm > 3000) rpm = 3000;
    
    // Calculate actual cutting speed achieved
    float actualSpeed;
    if (isMetric) {
        actualSpeed = (rpm * 3.14159f * diameter) / 1000.0f;
    } else {
        actualSpeed = (rpm * 3.14159f * diameter) / 12.0f;
    }
    
    // Build recommendation text
    String materialName = getMaterialName(material);
    String toolName = getToolName(tool);
    String operationName = getOperationName(operation);
    
    result.rpm = rpm;
    result.cuttingSpeed = actualSpeed;
    result.isValid = true;
    result.recommendation = materialName + " + " + toolName + " + " + operationName + 
                           " = " + String(rpm) + " RPM (" + String(actualSpeed, 0) + 
                           (isMetric ? " m/min" : " SFM") + ")";
    
    return result;
}

MaterialCategory CuttingParameters::getMaterialByTensileStrength(float tensileStrengthMPa) const {
    for (int i = 0; i < MATERIAL_COUNT; i++) {
        if (tensileStrengthMPa >= MATERIAL_TENSILE_STRENGTHS[i][0] && 
            tensileStrengthMPa <= MATERIAL_TENSILE_STRENGTHS[i][1]) {
            return (MaterialCategory)i;
        }
    }
    return MATERIAL_MILD_STEEL; // Default fallback
}

MaterialCategory CuttingParameters::getMaterialByName(const String& materialName) const {
    String name = materialName;
    name.toLowerCase();
    
    if (name.indexOf("aluminum") >= 0 || name.indexOf("aluminium") >= 0) return MATERIAL_ALUMINUM;
    if (name.indexOf("brass") >= 0 || name.indexOf("bronze") >= 0 || name.indexOf("copper") >= 0) return MATERIAL_BRASS_BRONZE;
    if (name.indexOf("mild steel") >= 0 || name.indexOf("a36") >= 0 || name.indexOf("1018") >= 0) return MATERIAL_MILD_STEEL;
    if (name.indexOf("medium steel") >= 0 || name.indexOf("1045") >= 0 || name.indexOf("4140") >= 0) return MATERIAL_MEDIUM_STEEL;
    if (name.indexOf("hard steel") >= 0 || name.indexOf("tool steel") >= 0) return MATERIAL_HARD_STEEL;
    if (name.indexOf("stainless 300") >= 0 || name.indexOf("304") >= 0 || name.indexOf("316") >= 0) return MATERIAL_STAINLESS_300;
    if (name.indexOf("stainless 400") >= 0 || name.indexOf("410") >= 0 || name.indexOf("420") >= 0) return MATERIAL_STAINLESS_400;
    if (name.indexOf("titanium") >= 0) return MATERIAL_TITANIUM;
    if (name.indexOf("inconel") >= 0 || name.indexOf("hastelloy") >= 0) return MATERIAL_INCONEL;
    if (name.indexOf("cast iron") >= 0) return MATERIAL_CAST_IRON;
    if (name.indexOf("plastic") >= 0) return MATERIAL_PLASTIC;
    if (name.indexOf("wood") >= 0) return MATERIAL_WOOD;
    
    return MATERIAL_MILD_STEEL; // Default fallback
}

ToolType CuttingParameters::getToolByName(const String& toolName) const {
    String name = toolName;
    name.toLowerCase();
    
    if (name.indexOf("hss") >= 0) return TOOL_HSS;
    if (name.indexOf("carbide") >= 0 && name.indexOf("coat") < 0) return TOOL_CARBIDE_UNCOATED;
    if (name.indexOf("carbide") >= 0 && name.indexOf("coat") >= 0) return TOOL_CARBIDE_COATED;
    if (name.indexOf("cbn") >= 0) return TOOL_CBN;
    if (name.indexOf("diamond") >= 0) return TOOL_DIAMOND;
    if (name.indexOf("ceramic") >= 0) return TOOL_CERAMIC;
    
    return TOOL_CARBIDE_COATED; // Default fallback
}

String CuttingParameters::getMaterialName(MaterialCategory material) const {
    switch (material) {
        case MATERIAL_ALUMINUM: return "Aluminum";
        case MATERIAL_BRASS_BRONZE: return "Brass/Bronze";
        case MATERIAL_MILD_STEEL: return "Mild Steel";
        case MATERIAL_MEDIUM_STEEL: return "Medium Steel";
        case MATERIAL_HARD_STEEL: return "Hard Steel";
        case MATERIAL_STAINLESS_300: return "Stainless 300";
        case MATERIAL_STAINLESS_400: return "Stainless 400";
        case MATERIAL_TITANIUM: return "Titanium";
        case MATERIAL_INCONEL: return "Inconel";
        case MATERIAL_CAST_IRON: return "Cast Iron";
        case MATERIAL_PLASTIC: return "Plastic";
        case MATERIAL_WOOD: return "Wood";
        default: return "Unknown";
    }
}

String CuttingParameters::getToolName(ToolType tool) const {
    switch (tool) {
        case TOOL_HSS: return "HSS";
        case TOOL_CARBIDE_UNCOATED: return "Carbide";
        case TOOL_CARBIDE_COATED: return "Coated Carbide";
        case TOOL_CBN: return "CBN";
        case TOOL_DIAMOND: return "Diamond";
        case TOOL_CERAMIC: return "Ceramic";
        default: return "Unknown";
    }
}

String CuttingParameters::getOperationName(OperationType operation) const {
    switch (operation) {
        case OP_ROUGH_TURNING: return "Rough Turn";
        case OP_FINISH_TURNING: return "Finish Turn";
        case OP_FACING: return "Face";
        case OP_THREADING: return "Thread";
        case OP_PARTING: return "Part";
        default: return "Unknown";
    }
}

bool CuttingParameters::isValidDiameter(float diameter) const {
    return diameter >= getMinDiameter() && diameter <= getMaxDiameter();
}

bool CuttingParameters::isValidMaterial(MaterialCategory material) const {
    return material >= 0 && material < MATERIAL_COUNT;
}

bool CuttingParameters::isValidTool(ToolType tool) const {
    return tool >= 0 && tool < TOOL_COUNT;
}

bool CuttingParameters::isValidOperation(OperationType operation) const {
    return operation >= 0 && operation < OP_COUNT;
}

String* CuttingParameters::getMaterialNames() const {
    static String names[MATERIAL_COUNT];
    for (int i = 0; i < MATERIAL_COUNT; i++) {
        names[i] = getMaterialName((MaterialCategory)i);
    }
    return names;
}

String* CuttingParameters::getToolNames() const {
    static String names[TOOL_COUNT];
    for (int i = 0; i < TOOL_COUNT; i++) {
        names[i] = getToolName((ToolType)i);
    }
    return names;
}

String* CuttingParameters::getOperationNames() const {
    static String names[OP_COUNT];
    for (int i = 0; i < OP_COUNT; i++) {
        names[i] = getOperationName((OperationType)i);
    }
    return names;
} 