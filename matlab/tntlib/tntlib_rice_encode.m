function c = tntlib_rice_encode(n,p)
%TNTLIB_RICE_ENCODE - Rice code decimal number n>=0 with Rice parameter p
%
%   c = TNTLIB_RICE_ENCODE(n,p)
%
%   Input : n - Nonnegative integer to be encoded
%           p - Rice parameter (must be a power of 2)
%   Output: c - Row vector containing binary Rice code
    
    if mod(n,1) ~= 0 || n < 0
        error('n must be a nonnegative integer');
    end
    
    if mod(p,2) ~= 0 || p < 0
        error('p must be a power of 2 and nonnegative');
    end
                    % Faster C style: 
    q = fix(n / p); % q = n >> p
    r = mod(n,p);   % r = n & (b - 1)
    
    c = [ones(1,q),0];        %< write the quotient in unary coding
    c = [c,fliplr(de2bi(r))]; %< remainder in truncated binary encoding
end

