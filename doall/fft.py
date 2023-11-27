import numpy as np
from cmath import exp, pi

N = 32
Nh = N >> 1
Nq = Nh>>1
s0 = 1
half = Nh

l2 = int(np.log2(N))
if l2 % 1 > 0 :
    raise ValueError("must be a power of 2")
else :
    print ("Log2 of N is %d" % l2)

#Create random array and reference array
#random_array = np.random.random(N)
random_array = np.arange(N,dtype=float)
reference_array = np.fft.fft(random_array.reshape((1,N)) )
print("Original array")
print(random_array)
print("-----")

#We use 2 full arrays of N (one for real and one for imaginary), and 2 half arrays (size N/2)
XR = random_array
XI = np.zeros(N, dtype=float)
XauxR = np.zeros(Nh,dtype=float)
XauxI = np.zeros(Nh,dtype=float)

#Create the factor array of size N/2
#We stream these factors to the PEs. Idk if there is a way to reduce this more (from N/2 to less)
#but this is what I came up with so far
exponent = np.pi * np.arange(half) / half
fI = np.sin(exponent)
fR = np.cos(exponent)
print(fR.astype(np.float16))
print(fI.astype(np.float16))
#So to the PEs we stream the input array XR (of floats and size N), and factor arrays (2 arrays of size N/2)
#Internally the PE will use 3N memory (contiguously allocated), all memory is accessed as 1D arrays
print("-------")
for it in reversed(range(l2)):
    print("\n++++%d+++" % it)
    print("S0xS1 = %dx%d" % (s0,half*2))

    for i in range(s0):
        idx = i<<it #i*half
        c1a = fR[idx]
        #c1b = -fI[idx]
        c1b = fI[idx]
        for j in range(half):
            XauxR[idx+j] = c1a * XR[Nh+idx+j]
            XauxI[idx+j] = c1a * XI[Nh+idx+j]
            #
            XR[Nh+idx+j]*= c1b
            XI[Nh+idx+j]*= c1b
            #
            XauxR[idx+j]+= XI[Nh+idx+j]
            XauxI[idx+j]-= XR[Nh+idx+j]

    for i in range(Nh):
        XR[i+Nh] = XR[i] - XauxR[i]
        XI[i+Nh] = XI[i] - XauxI[i]
        XR[i] += XauxR[i]
        XI[i] += XauxI[i]

    for i in range(Nh):
        XauxR[i] = XR[i+Nh]
        XauxI[i] = XI[i+Nh]

    if (it!=0):
        #Fill Right part of the array
        stride = half
        half>>=1
        s0<<=1
        baseA = Nh;baseB = half
        while(baseB<Nh):
            for j in range(half):
                XR[j+baseA] = XR[j+baseB]
                XI[j+baseA] = XI[j+baseB]
            baseA +=half; baseB +=stride
        baseB = half
        while(baseB<Nh):
            for j in range(half):
                XR[j+baseA] = XauxR[j+baseB]
                XI[j+baseA] = XauxI[j+baseB]
            baseA +=half;baseB +=stride
        #Fill Left part of the array
        baseA = 0;baseB = 0
        while(baseB<Nh):
            for j in range(half):
                XR[j+baseA] = XR[j+baseB]
                XI[j+baseA] = XI[j+baseB]
            baseA +=half;baseB +=stride
        baseB = 0
        while(baseB<Nh):
            for j in range(half):
                XR[j+baseA] = XauxR[j+baseB]
                XI[j+baseA] = XauxI[j+baseB]
            baseA +=half;baseB +=stride
    else: 
        for i in range(Nh):
            XR[i+Nh] = XauxR[i]
            XI[i+Nh] = XauxI[i]

#Comparison with reference array
result_array = np.zeros(N, dtype=complex)
for i in range(N):
    result_array[i] = complex(XR[i],XI[i])
print(result_array)
print("Reference array")
print(reference_array)



