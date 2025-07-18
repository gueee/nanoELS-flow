#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <Arduino.h>
#include <cstddef>  // For size_t

/**
 * Real-time safe circular buffer for embedded systems
 * 
 * Features:
 * - Fixed-size allocation (no dynamic memory)
 * - ISR-safe operations
 * - O(1) constant time operations
 * - Thread-safe for single producer/single consumer
 * 
 * Template parameters:
 * - T: Type of elements to store
 * - N: Maximum number of elements (must be power of 2 for optimal performance)
 */
template<typename T, size_t N>
class CircularBuffer {
private:
    static_assert(N > 0, "Buffer size must be greater than 0");
    static_assert((N & (N - 1)) == 0, "Buffer size must be power of 2 for optimal performance");
    
    // Buffer storage - aligned for cache efficiency
    alignas(4) T buffer[N];
    
    // Index management - volatile for ISR safety
    volatile size_t head;    // Write index
    volatile size_t tail;    // Read index
    volatile size_t count;   // Current element count
    
    // Bitmask for efficient modulo operation (works because N is power of 2)
    static constexpr size_t mask = N - 1;
    
public:
    /**
     * Constructor - initializes empty buffer
     */
    CircularBuffer() : head(0), tail(0), count(0) {}
    
    /**
     * Add element to buffer (producer operation)
     * @param item Element to add
     * @return true if successful, false if buffer full
     * @note ISR-safe, constant time O(1)
     */
    bool push(const T& item) {
        // Check if buffer is full
        if (count >= N) {
            return false;  // Buffer overflow
        }
        
        // Add item at head position
        buffer[head] = item;
        
        // Advance head with wrap-around (efficient with power-of-2 size)
        head = (head + 1) & mask;
        
        // Update count atomically (important for ISR safety)
        count++;
        
        // Update peak utilization tracking
        updatePeakCount();
        
        return true;
    }
    
    /**
     * Remove element from buffer (consumer operation)
     * @param item Reference to store the removed element
     * @return true if successful, false if buffer empty
     * @note ISR-safe, constant time O(1)
     */
    bool pop(T& item) {
        // Check if buffer is empty
        if (count == 0) {
            return false;  // Buffer underflow
        }
        
        // Get item from tail position
        item = buffer[tail];
        
        // Advance tail with wrap-around
        tail = (tail + 1) & mask;
        
        // Update count atomically
        count--;
        
        return true;
    }
    
    /**
     * Peek at front element without removing it
     * @param item Reference to store the front element
     * @return true if successful, false if buffer empty
     * @note ISR-safe, constant time O(1)
     */
    bool front(T& item) const {
        if (count == 0) {
            return false;
        }
        
        item = buffer[tail];
        return true;
    }
    
    /**
     * Check if buffer is empty
     * @return true if empty
     * @note ISR-safe, constant time O(1)
     */
    bool empty() const {
        return count == 0;
    }
    
    /**
     * Check if buffer is full
     * @return true if full
     * @note ISR-safe, constant time O(1)
     */
    bool full() const {
        return count >= N;
    }
    
    /**
     * Get current number of elements
     * @return Number of elements in buffer
     * @note ISR-safe, constant time O(1)
     */
    size_t size() const {
        return count;
    }
    
    /**
     * Get maximum buffer capacity
     * @return Maximum number of elements
     * @note Compile-time constant
     */
    static constexpr size_t capacity() {
        return N;
    }
    
    /**
     * Clear all elements from buffer
     * @note ISR-safe, constant time O(1)
     */
    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }
    
    /**
     * Get buffer utilization percentage
     * @return Utilization as percentage (0-100)
     * @note Useful for monitoring and tuning
     */
    float utilization() const {
        return (static_cast<float>(count) / N) * 100.0f;
    }
    
    /**
     * Get peak utilization since last reset
     * @return Peak utilization count
     * @note For performance monitoring
     */
    size_t getPeakUtilization() const {
        return peakCount;
    }
    
    /**
     * Reset peak utilization counter
     */
    void resetPeakUtilization() {
        peakCount = 0;
    }

private:
    // Performance monitoring
    mutable size_t peakCount = 0;
    
    // Update peak count (called internally)
    void updatePeakCount() const {
        if (count > peakCount) {
            peakCount = count;
        }
    }
};

// Convenience typedefs for common buffer sizes
template<typename T>
using CircularBuffer16 = CircularBuffer<T, 16>;

template<typename T>
using CircularBuffer32 = CircularBuffer<T, 32>;

template<typename T>
using CircularBuffer64 = CircularBuffer<T, 64>;

template<typename T>
using CircularBuffer128 = CircularBuffer<T, 128>;

#endif // CIRCULARBUFFER_H