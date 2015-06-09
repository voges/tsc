function H = tntlib_whtmtx(n)
%TNTLIB_WHTMTX - Unitary Walsh-Hadamard transform matrix
%
%   H = TNTLIB_WHTMTX(n)
%
%   Input : n - Size of WHT matrix (n must be a power of 2)
%   Output: H - WHT matrix of size (n,n)

    if mod(log2(n),1) ~= 0 || n < 2
        error('n must be a power of 2 and greater than 1');
    end
    
    % Compose WH-matrix
    H = zeros(n);
    H(1:2,1:2) = [1,1;1,-1];
    i = 4;
    while i <= n
        H_prev = H(1:i/2,1:i/2);
        H(1:i,1:i) = [H_prev,H_prev;H_prev,-H_prev];
        i = i * 2;
    end
    
    % Make H a unitary transform (H * H' = I)
    H = 1/sqrt(n) * H;
end

