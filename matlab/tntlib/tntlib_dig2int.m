function int = tntlib_dig2int(digits)
%TNTLIB_DIG2INT - Conversion from decimal digits to integer
%
%   int = TNTLIB_DIG2INT(digits)
%
%   Input : digits - Vector holding the decimal digits, all digits must be
%                    positive.
%   Output: int    - Integer

    n = length(digits);

    if sum(digits >= 0) < n
        error('Only nonnegative digits allowed!');
    end

    int = 0;
    for i = 1:n
        int = int + digits(i) * 10^(n - i);
    end
end

