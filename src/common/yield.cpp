#include <iostream>
#include <cmath>
#include <cassert>

#define REDUNDANCY_ALLOWED 1

int dieYieldData(double die_width_mm, double die_height_mm) {
    //0.2mm scribes, 4mm edge loss, and 0.07 defects per mm
    const double scribe = 0.2;
    const double edge_loss_mm = 4;
    const double defect_density_num_per_sqcm = 0.07;
    const double wafer_diameter = 300.0;
    const double max_rectangular_length_wafer = 215.0;
    assert(die_width_mm > 0); assert(die_height_mm > 0);
    // assert(die_width_mm <= max_rectangular_length_wafer);
    // assert(die_height_mm <= max_rectangular_length_wafer);
    if (die_width_mm > max_rectangular_length_wafer) std::cout << "ERROR: die_width_mm: " << die_width_mm << " beyond max_rectangular_length_wafer: " << max_rectangular_length_wafer << endl;
    if (die_height_mm > max_rectangular_length_wafer) std::cout << "ERROR: die_height_mm: " << die_height_mm << " beyond max_rectangular_length_wafer: " << max_rectangular_length_wafer << endl;

    bool hardware_redundancy = false;
    double min_dies_per_wafer = 0;
    #if REDUNDANCY_ALLOWED
        // Assume that after 400mm2, the tiled hardware has redundancy so that defects are solved in software.
        // Redundancy increases, e.g., each dimension by 10%.
        if (die_width_mm * die_height_mm > 400) {
            die_width_mm *= 1.1;
            die_height_mm *= 1.1;
            hardware_redundancy = true;
            min_dies_per_wafer = 1;
        }
    #endif

    // Cases where only one die fits on the wafer
    double max_side_len = std::max(die_width_mm, die_height_mm);
    if (max_side_len * 2 > max_rectangular_length_wafer) return min_dies_per_wafer;

    double die_area_without_scribe = die_width_mm * die_height_mm;
    double average_side = std::sqrt(die_area_without_scribe) + scribe;
    double die_area_square = std::pow(average_side, 2);

    double diameter_edge_loss = wafer_diameter - (2 * edge_loss_mm);
    double area_4_wafer = (M_PI * std::pow(diameter_edge_loss, 2));
    double area_4_dies = 4 * die_area_square;
    double perimeter = M_PI * diameter_edge_loss;
    double dies_on_square_wafer = (area_4_wafer / area_4_dies);
    //std::cout << "Dies on square wafer: " << dies_on_square_wafer << "\n";
    double length_die_twice_length = std::sqrt(2 * die_area_square);
    //std::cout << "Perimeter: " << perimeter << "\n";
    double crossing_dies = (perimeter / length_die_twice_length);
    //std::cout << "Crossing dies: " << crossing_dies << "\n";
    // Set 4 as the minimum number of dies per wafer 
    double max_die_per_wafer =  std::max(dies_on_square_wafer - crossing_dies, 4 * min_dies_per_wafer);

    double yield = 1;
    if (!hardware_redundancy){
        double die_area_without_scribe_cm = die_area_without_scribe * 0.01;
        yield = std::pow(((1 - (std::exp(-(die_area_without_scribe_cm * defect_density_num_per_sqcm)))) / (die_area_without_scribe_cm * defect_density_num_per_sqcm)), 2);
        double yield_round = (std::round(yield * 1000) / 1000) * 100;
        // std::cout << "Yield (%): " << yield_round << std::endl;
    }

    double good_die_per_wafer = yield * max_die_per_wafer;
    // Outputs
    int max_die_per_wafer_round = std::round(max_die_per_wafer);
    int good_die_per_wafer_round = std::round(good_die_per_wafer);
    //std::cout << "Max die per wafer: " << max_die_per_wafer_round << std::endl;
    return good_die_per_wafer_round;
}


int test_compute_die() {
    double die_width_mm = 13.5;
    double die_height_mm = 12.5;
    int good_die_per_wafer = dieYieldData(die_width_mm, die_height_mm);
    assert(good_die_per_wafer == 299);
    std::cout << "Good die per wafer: " << good_die_per_wafer << std::endl;
    return 0;
}

int test_io() {
    double die_width_mm = 4;
    double die_height_mm = 10;
    int good_die_per_wafer = dieYieldData(die_width_mm, die_height_mm);
    std::cout << "Good die per wafer: " << good_die_per_wafer << std::endl << std::flush;
    assert(good_die_per_wafer == 1433);
    return 0;
}

int test_sweep(){
    for(double size = 10.0; size <= 150.0; size += 1.0) {
        int yield = dieYieldData(size, size);
        std::cout << "For die size " << size << "x" << size << ", Good Die Per Wafer: " << yield << std::endl;
    }
    return 0;
}

//int main() {return test_sweep();}