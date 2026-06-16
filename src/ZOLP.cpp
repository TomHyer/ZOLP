// ZOLP.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include "Tests.h"

int main()
{
	bool success = ZOLP::Test::RunAll(std::cerr);
    std::cout << "Tests: " << success << "\n";
	return !success;
}
