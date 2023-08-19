#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        if(buffer_ != nullptr)
            Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept { 
        Deallocate(buffer_);
        buffer_ = nullptr;
        capacity_ = 0;

        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);

     }

     RawMemory& operator=(RawMemory&& rhs) noexcept { 
        Deallocate(buffer_);
        buffer_ = nullptr;
        capacity_ = 0;

        std::swap(buffer_, rhs.buffer_);
        std::swap(capacity_, rhs.capacity_);

        return *this;
     }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};




template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept{
        return data_.GetAddress();
    }

    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept{
        return data_.GetAddress() + size_;
    }

    Vector() = default;

    explicit Vector(size_t size): data_(size), size_(size)  {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);    
    }

    Vector(const Vector& other): data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept :data_(std::move(other.data_)), size_(std::move(other.size_)){ 
        other.size_ = 0;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        assert(pos >= begin() && pos <= end());

        if(size_ == Capacity()){

            size_t itpos = pos - begin();
            RawMemory<T> newmemory(size_ == 0 ? 1 : size_ * 2);
            Construct(newmemory.GetAddress() + itpos, std::forward<Args>(args)...);
            try{
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), itpos, newmemory.GetAddress());
                    std::uninitialized_move_n(data_.GetAddress() + itpos , end() - pos, newmemory.GetAddress() + itpos + 1);
                
                } else{
                    std::uninitialized_copy_n(data_.GetAddress(), itpos, newmemory.GetAddress());
                    std::uninitialized_copy_n(data_.GetAddress() + itpos , end() - pos, newmemory.GetAddress() + itpos + 1);
                }
            }
            catch(...){
                std::destroy_at(newmemory.GetAddress() + itpos);
                std::destroy_n(data_.GetAddress(), size_);
                throw;
            }
            data_.Swap(newmemory);
            std::destroy_n(newmemory.GetAddress(), size_);
            size_++;

            return begin() + itpos;
        }

        size_t itpos = pos - begin();

        if(pos == cend()){
            Construct(end(), std::forward<Args>(args)...);
            size_++;
            return  const_cast<iterator>(pos);
        } 
        else{
            T temp(std::forward<Args>(args)...);
            Construct(end(), std::move(*(end() - 1)));
            try{
                std::move_backward(const_cast<iterator>(pos), end() - 1, end() );
            }
            catch(...){
                std::destroy_at(end());
            }
            *(begin() + itpos) = std::move(temp);
                          
            size_++;
            return const_cast<iterator>(pos);
        }

    }
    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos <= end());

        for(auto it = const_cast<iterator>(pos); it != end() - 1; it++){
            *it = std::move(*(it + 1));
        }

        PopBack();

        return const_cast<iterator>(pos) ;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

    Vector& operator=(const Vector& rhs){
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector temp(rhs);
                Swap(temp);
            } else {
                if(Size() > rhs.Size()){
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.Size(), data_.GetAddress()); 
                    std::destroy_n(data_.GetAddress() + rhs.Size(), Size() - rhs.Size());
                } else{
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + Size(), data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + Size(), rhs.Size() - Size(), data_.GetAddress());
                }

            size_ = rhs.Size();
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept{
        
        std::swap(data_, rhs.data_);
        //data_ = std::move(rhs.data_);
        size_ = rhs.size_;
        rhs.size_ = 0;
        return *this;
    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }


    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(CreateCopy(new_capacity));
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), Size());
    }

    void Resize(size_t new_size){
        if(new_size > size_){
            size_t old_size = size_;
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + old_size, new_size - old_size);
        }  else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size); 
        }

        size_ = new_size;
    }

    void PushBack(const T& value){
        EmplaceBack(value);
    }

    void PushBack(T&& value){
        EmplaceBack(std::move(value));
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        return *Emplace(end(), std::forward<Args>(args)...);
    }
    

    void PopBack()  noexcept {
        assert(size_ > 0);
        Destroy(data_.GetAddress() + size_ - 1);
        size_--;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    

private:

    RawMemory<T> CreateCopy(size_t capacity){
        RawMemory<T> new_data(capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), Size(), new_data.GetAddress());
        }
        else{
            std::uninitialized_copy_n(data_.GetAddress(), Size(), new_data.GetAddress());
        }

        return new_data;
    }

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    template<typename... Type>
    static void Construct(T* buf, Type&&... elem) {
        new (buf) T(std::forward<Type>(elem)...);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }


    RawMemory<T> data_;
    size_t size_ = 0;
};