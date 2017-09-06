function ABCDEF = fit_conic(x, y)
% Conic section: A*x^2 + B*x*y + C*y^2 + D*x + E*y + F = 0.
% More than 5 points are required.

%ABCDEF = fit_conic_by_linear_regression(x, y);
ABCDEF = fit_conic_by_sampson_approximation(x, y);

return;

%-----------------------------------------------------------

function ABCDEF = fit_conic_by_linear_regression(x, y)
% Use linear regression.
% REF [book] >> "Multiple View Geometry in Computer Vision", p. 31.

[U, S, V] = svd([x.^2 x.*y y.^2 x y ones(size(x))]);
ABCDEF = V(:,end)';

%if abs(ABCDEF(1)) > eps
%	ABCDEF = ABCDEF / ABCDEF(1);
%elseif abs(ABCDEF(3)) > eps
%	ABCDEF = ABCDEF / ABCDEF(3);
%end;

return;

%-----------------------------------------------------------

function ABCDEF = fit_conic_by_sampson_approximation(x, y)
% Use the Sampson approximation to the geometric distance for a conic.
% REF [book] >> "Multiple View Geometry in Computer Vision", p. 99.

ABCDEF0 = fit_conic_by_linear_regression(x, y);

options = optimset('TolX', 1e-6, 'TolFun', 1e-6);
ABCDEF = fminsearch(@sampson_approximation_cost, ABCDEF0, options, [x y ones(size(x))]');

return;

%-----------------------------------------------------------

function cost = sampson_approximation_cost(ABCDEF, X)

C = conic_poly2mat(ABCDEF);
len = size(X, 2);
cost = 0;
for ii = 1:len
	CX = C * X(:,ii);
	cost = cost + 0.25 * (X(:,ii)' * CX)^2 / (CX(1)^2 + CX(2)^2);
end;

return;